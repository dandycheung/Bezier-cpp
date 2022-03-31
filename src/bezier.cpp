#include "Bezier/bezier.h"
#include "Bezier/legendre_gauss.h"

#include <numeric>

#include <unsupported/Eigen/MatrixFunctions>
#include <unsupported/Eigen/Polynomials>

using namespace Bezier;

inline double factorial(uint k) { return std::tgamma(k + 1); }
inline double binomial(uint n, uint k) { return factorial(n) / (factorial(k) * factorial(n - k)); }
inline Eigen::VectorXd trimZeroes(const Eigen::VectorXd& vec)
{
  auto idx = vec.size();
  while (idx && vec(idx - 1) == 0.0)
    --idx;
  return vec.head(idx);
}

Curve::Curve(Eigen::MatrixX2d points)
    : control_points_(std::move(points)), N_(static_cast<uint>(control_points_.rows()))
{
}

Curve::Curve(const PointVector& points)
{
  N_ = static_cast<uint>(points.size());
  control_points_.resize(N_, 2);
  for (uint k = 0; k < N_; k++)
    control_points_.row(k) = points[k];
}

Curve& Curve::operator=(const Curve& curve)
{
  control_points_ = curve.control_points_;
  resetCache();
  return *this;
}

uint Curve::order() const { return N_ - 1; }

PointVector Curve::controlPoints() const
{
  PointVector points(static_cast<unsigned long>(N_));
  for (uint k = 0; k < N_; k++)
    points[k] = control_points_.row(k);
  return points;
}

Point Curve::controlPoint(uint idx) const { return control_points_.row(idx); }

std::pair<Point, Point> Curve::endPoints() const { return {control_points_.row(0), control_points_.row(N_ - 1)}; }

PointVector Curve::polyline(double flatness) const
{
  if (!cached_polyline_ || std::fabs(cached_polyline_flatness_ - flatness) >= 1.e-10)
  {
    auto polyline = new PointVector;
    polyline->emplace_back(control_points_.row(0));
    if (N_ == 2)
    {
      polyline->emplace_back(control_points_.row(1));
    }
    else
    {
      std::vector<Eigen::MatrixX2d> subcurves;
      subcurves.emplace_back(control_points_);

      Eigen::ArrayXd X(Eigen::Index(N_ - 2));
      Eigen::ArrayXd Y(Eigen::Index(N_ - 2));
      Eigen::ArrayXd l = Eigen::ArrayXd::LinSpaced(N_ - 2, 1, N_ - 2);
      Eigen::ArrayXd b = l.unaryExpr([n = N_ - 1](uint k) { return binomial(n, k); });

      while (!subcurves.empty())
      {
        auto& cp = subcurves.back();

        auto step = (cp.row(N_ - 1) - cp.row(0)) / (N_ - 1);

        X = (b * (cp.block(1, 0, N_ - 2, 1).array() - cp(0, 0) - l * step(0))).square();
        Y = (b * (cp.block(1, 1, N_ - 2, 1).array() - cp(0, 1) - l * step(1))).square();

        if (X.maxCoeff() + Y.maxCoeff() <= 16 * flatness * flatness)
        {
          subcurves.pop_back();
          polyline->emplace_back(cp.row(N_ - 1));
        }
        else
        {
          Eigen::MatrixX2d ncp1 = splittingCoeffsRight(N_) * cp;
          Eigen::MatrixX2d ncp2 = splittingCoeffsLeft(N_) * cp;
          subcurves.pop_back();
          subcurves.emplace_back(std::move(ncp1));
          subcurves.emplace_back(std::move(ncp2));
        }
      }
    }

    const_cast<Curve*>(this)->cached_polyline_flatness_ = flatness;
    const_cast<Curve*>(this)->cached_polyline_.reset(polyline);
  }
  return *cached_polyline_;
}

double Curve::length() const { return length(0.0, 1.0); }

double Curve::length(double t) const { return length(0.0, t); }

double Curve::length(double t1, double t2) const
{
  double sum = 0;

  for (uint k = 0; k < LegendreGauss::N; k++)
    sum += LegendreGauss::weights[k] * derivativeAt(LegendreGauss::abcissae[k] * (t2 - t1) / 2 + (t1 + t2) / 2).norm();

  return sum * (t2 - t1) / 2;
}

double Curve::iterateByLength(double t, double s, double epsilon) const
{

  const double s_t = length(t);

  if (s_t + s < 0)
    return 0;
  if (s_t + s > length())
    return 1;

  double f{-s}, f_d{};

  while (std::fabs(f) > epsilon)
  {
    // Halley
    f_d = derivativeAt(t).norm();
    t -= (2 * f * f_d) / (2 * f_d * f_d - f * derivativeAt(2, t).norm());
    f = (length(t) - s_t - s);
  }

  return t;
}

void Curve::reverse()
{
  control_points_ = control_points_.colwise().reverse().eval();
  resetCache();
}

void Curve::moveControlPoint(uint idx, const Point& point)
{
  control_points_.row(idx) = point;
  resetCache();
}

void Curve::manipulateCurvature(double t, const Point& point)
{
  if (N_ < 3 || N_ > 4)
    throw std::logic_error{"Only quadratic and cubic curves can be manipulated"};

  double r =
      std::fabs((std::pow(t, N_ - 1) + std::pow(1 - t, N_ - 1) - 1) / (std::pow(t, N_ - 1) + std::pow(1 - t, N_ - 1)));
  double u = std::pow(1 - t, N_ - 1) / (std::pow(t, N_ - 1) + std::pow(1 - t, N_ - 1));
  Point C = u * control_points_.row(0) + (1 - u) * control_points_.row(N_ - 1);
  const Point& B = point;
  Point A = B - (C - B) / r;

  switch (N_)
  {
  case 3:
    control_points_.row(1) = A;
    break;
  case 4:
    Point e1 = control_points_.row(0) * std::pow(1 - t, 2) + control_points_.row(1) * 2 * t * (1 - t) +
               control_points_.row(2) * std::pow(t, 2);
    Point e2 = control_points_.row(1) * std::pow(1 - t, 2) + control_points_.row(2) * 2 * t * (1 - t) +
               control_points_.row(3) * std::pow(t, 2);
    e1 = B + e1 - valueAt(t);
    e2 = B + e2 - valueAt(t);
    Point v1 = A - (A - e1) / (1 - t);
    Point v2 = A + (e2 - A) / t;
    control_points_.row(1).noalias() = control_points_.row(0) + (v1.transpose() - control_points_.row(0)) / t;
    control_points_.row(2).noalias() = control_points_.row(3) - (control_points_.row(3) - v2.transpose()) / (1 - t);
  }
  resetCache();
}

void Curve::elevateOrder()
{
  control_points_ = elevateOrderCoeffs(static_cast<uint>(N_)) * control_points_;
  resetCache();
}

void Curve::lowerOrder()
{
  if (N_ == 2)
    throw std::logic_error{"Cannot further reduce the order of curve."};
  control_points_ = lowerOrderCoeffs(static_cast<uint>(N_)) * control_points_;
  resetCache();
}

Point Curve::valueAt(double t) const
{
  if (N_ == 0)
    return {0, 0};
  return (Eigen::VectorXd(Eigen::pow(t, Eigen::ArrayXd::LinSpaced(N_, 0, N_ - 1))).transpose() * bernsteinCoeffs(N_) *
          control_points_)
      .transpose();
}

PointVector Curve::valueAt(const std::vector<double>& t_vector) const
{
  PointVector points;
  points.reserve(t_vector.size());

  auto t_matrix =
      Eigen::Map<const Eigen::VectorXd>(t_vector.data(), static_cast<int>(t_vector.size())).replicate(1, N_);
  auto p_matrix = Eigen::ArrayXd::LinSpaced(N_, 0, N_ - 1).transpose().replicate(static_cast<int>(t_vector.size()), 1);
  Eigen::MatrixXd power_basis = Eigen::pow(t_matrix.array(), p_matrix.array());
  Eigen::MatrixXd points_eigen = (power_basis * bernsteinCoeffs(N_) * control_points_);

  for (uint k = 0; k < points_eigen.rows(); k++)
    points.emplace_back(points_eigen.row(k));

  return points;
}

double Curve::curvatureAt(double t) const
{
  Vector d1 = derivativeAt(t);
  Vector d2 = derivativeAt(2, t);

  return (d1.x() * d2.y() - d1.y() * d2.x()) / std::pow(d1.norm(), 3);
}

double Curve::curvatureDerivativeAt(double t) const
{
  Vector d1 = derivativeAt(t);
  Vector d2 = derivativeAt(2, t);
  Vector d3 = derivativeAt(3, t);

  return (d1.x() * d3.y() - d1.y() * d3.x()) / std::pow(d1.norm(), 3) -
         3 * d1.dot(d2) * (d1.x() * d2.y() - d1.y() * d2.x()) / std::pow(d1.norm(), 5);
}

Vector Curve::tangentAt(double t, bool normalize) const
{
  Point p(derivativeAt(t));
  if (normalize && p.norm() > 0)
    p.normalize();
  return p;
}

Vector Curve::normalAt(double t, bool normalize) const
{
  Point tangent = tangentAt(t, normalize);
  return {-tangent.y(), tangent.x()};
}

const Curve& Curve::derivative() const
{
  if (!cached_derivative_)
  {
    const_cast<Curve*>(this)->cached_derivative_.reset(
        N_ == 1
            ? new Curve(PointVector{Point(0, 0)})
            : new Curve(((N_ - 1) * (control_points_.bottomRows(N_ - 1) - control_points_.topRows(N_ - 1))).eval()));
  }
  return *cached_derivative_;
}

const Curve& Curve::derivative(uint n) const
{
  if (n == 0)
    throw std::invalid_argument{"double 'n' cannot be zero."};
  auto nth_derivative = &derivative();
  for (uint k = 1; k < n; k++)
    nth_derivative = &nth_derivative->derivative();
  return *nth_derivative;
}

Vector Curve::derivativeAt(double t) const { return derivative().valueAt(t); }

Vector Curve::derivativeAt(uint n, double t) const { return derivative(n).valueAt(t); }

std::vector<double> Curve::roots() const
{
  if (!cached_roots_)
  {
    auto roots = new std::vector<double>();
    if (N_ > 1)
    {
      std::vector<double> roots_X, roots_Y;
      Eigen::MatrixXd bezier_polynomial = bernsteinCoeffs(N_) * control_points_;
      Eigen::PolynomialSolver<double, Eigen::Dynamic> poly_solver;
      auto trimmed = trimZeroes(bezier_polynomial.col(0));
      if (trimmed.size() > 1)
      {
        poly_solver.compute(trimmed);
        poly_solver.realRoots(roots_X);
      }
      trimmed = trimZeroes(bezier_polynomial.col(1));
      if (trimmed.size() > 1)
      {
        poly_solver.compute(trimmed);
        poly_solver.realRoots(roots_Y);
      }
      roots->reserve(roots_X.size() + roots_Y.size());
      std::copy_if(std::make_move_iterator(roots_X.begin()), std::make_move_iterator(roots_X.end()),
                   std::back_inserter(*roots), [](double t) { return t >= 0 && t <= 1; });
      std::copy_if(std::make_move_iterator(roots_Y.begin()), std::make_move_iterator(roots_Y.end()),
                   std::back_inserter(*roots), [](double t) { return t >= 0 && t <= 1; });
    }
    const_cast<Curve*>(this)->cached_roots_.reset(roots);
  }
  return *cached_roots_;
}

std::vector<double> Curve::extrema() const { return derivative().roots(); }

BoundingBox Curve::boundingBox() const
{
  if (!cached_bounding_box_)
  {
    PointVector extremes = valueAt(extrema());

    extremes.emplace_back(control_points_.row(0));
    extremes.emplace_back(control_points_.row(N_ - 1));

    // find mininum and maximum along each axis
    auto x_extremes = std::minmax_element(extremes.begin(), extremes.end(),
                                          [](const Point& lhs, const Point& rhs) { return lhs.x() < rhs.x(); });
    auto y_extremes = std::minmax_element(extremes.begin(), extremes.end(),
                                          [](const Point& lhs, const Point& rhs) { return lhs.y() < rhs.y(); });
    const_cast<Curve*>(this)->cached_bounding_box_.reset(new BoundingBox(
        Point(x_extremes.first->x(), y_extremes.first->y()), Point(x_extremes.second->x(), y_extremes.second->y())));
  }
  return *cached_bounding_box_;
}

std::pair<Curve, Curve> Curve::splitCurve(double z) const
{
  return {Curve(splittingCoeffsLeft(N_, z) * control_points_), Curve(splittingCoeffsRight(N_, z) * control_points_)};
}

PointVector Curve::intersections(const Curve& curve, double epsilon) const
{
  PointVector points_of_intersection;

  std::vector<std::pair<Eigen::MatrixX2d, Eigen::MatrixX2d>> subcurve_pairs;

  if (this != &curve)
  {
    subcurve_pairs.emplace_back(control_points_, curve.control_points_);
  }
  else
  {
    // self intersections

    // divide curve into subcurves at roots
    auto t = extrema();
    std::sort(t.begin(), t.end());
    std::vector<Eigen::MatrixX2d> subcurves;
    for (uint k = 0; k < t.size(); k++)
    {
      if (subcurves.empty())
      {
        subcurves.emplace_back(splittingCoeffsLeft(N_, t[k] - epsilon / 2) * control_points_);
        subcurves.emplace_back(splittingCoeffsRight(N_, t[k] + epsilon / 2) * control_points_);
      }
      else
      {
        auto new_cp = subcurves.back();
        subcurves.pop_back();
        subcurves.emplace_back(splittingCoeffsLeft(N_, t[k] - epsilon / 2) * new_cp);
        subcurves.emplace_back(splittingCoeffsRight(N_, t[k] + epsilon / 2) * new_cp);
      }

      std::for_each(t.begin() + k + 1, t.end(), [t = t[k]](double& x) { x = (x - t) / (1 - t); });
    }

    // create all pairs of subcurves
    for (uint k = 0; k < subcurves.size(); k++)
      for (uint i = k + 1; i < subcurves.size(); i++)
        subcurve_pairs.emplace_back(subcurves[k], subcurves[i]);
  }

  auto bbox = [](const Eigen::MatrixX2d& cp) {
    return BoundingBox(Point(cp.col(0).minCoeff(), cp.col(1).minCoeff()),
                       Point(cp.col(0).maxCoeff(), cp.col(1).maxCoeff()));
  };

  while (!subcurve_pairs.empty())
  {
    auto [part_a, part_b] = subcurve_pairs.back();
    subcurve_pairs.pop_back();

    BoundingBox bbox1 = bbox(part_a);
    BoundingBox bbox2 = bbox(part_b);

    if (!bbox1.intersects(bbox2))
      // no intersection
      continue;

    if (bbox1.diagonal().norm() < epsilon && bbox2.diagonal().norm() < epsilon)
    {
      // segments converged, check if not already found and add new
      Point new_point = bbox1.center();
      if (std::none_of(points_of_intersection.begin(), points_of_intersection.end(),
                       [new_point, epsilon](const Point& point) { return (point - new_point).norm() < epsilon; }))
        points_of_intersection.emplace_back(std::move(new_point));

      continue;
    }

    // intersection exists, but segments are still too large
    // divide both segments in half and new pairs
    std::vector<Eigen::MatrixX2d> subcurves_a;
    std::vector<Eigen::MatrixX2d> subcurves_b;

    if (bbox1.diagonal().norm() < epsilon)
      // if small enough, do not divide it further
      subcurves_a.emplace_back(part_a);
    else
    {
      // divide into two subcurves
      // first insert 2nd subcurve t = [0.5 to 1]
      subcurves_a.emplace_back(splittingCoeffsRight(N_) * part_a);
      subcurves_a.emplace_back(splittingCoeffsLeft(N_) * part_a);
    }

    if (bbox2.diagonal().norm() < epsilon)
      // if small enough, do not divide it further
      subcurves_b.emplace_back(part_b);
    else
    {
      // divide into two subcurves
      // first insert 2nd subcurve t = [0.5 to 1]
      subcurves_b.emplace_back(splittingCoeffsRight(static_cast<uint>(part_b.rows())) * part_b);
      subcurves_b.emplace_back(splittingCoeffsLeft(static_cast<uint>(part_b.rows())) * part_b);
    }

    // insert all combinations for next iteration
    // last pair is one where both subcurves have smalles t ranges
    for (auto& subcurve_b : subcurves_b)
      for (auto& subcurve_a : subcurves_a)
        subcurve_pairs.emplace_back(subcurve_a, subcurve_b);
  }

  return points_of_intersection;
}

double Curve::projectPoint(const Point& point) const
{
  if (!cached_projection_polynomial_part_)
  {
    Eigen::MatrixXd curve_polynomial = (bernsteinCoeffs(N_) * control_points_);
    Eigen::MatrixXd derivate_polynomial = (bernsteinCoeffs(N_ - 1) * derivative().control_points_);

    Eigen::VectorXd polynomial_part = Eigen::VectorXd::Zero(curve_polynomial.rows() + derivate_polynomial.rows() - 1);
    for (uint k = 0; k < curve_polynomial.rows(); k++)
      polynomial_part.middleRows(k, derivate_polynomial.rows()) +=
          derivate_polynomial * curve_polynomial.row(k).transpose();

    const_cast<Curve*>(this)->cached_projection_polynomial_part_.reset(new Eigen::VectorXd(std::move(polynomial_part)));
    const_cast<Curve*>(this)->cached_projection_polynomial_derivative_ = std::move(derivate_polynomial);
  }

  Eigen::VectorXd polynomial = *cached_projection_polynomial_part_;
  polynomial.topRows(cached_projection_polynomial_derivative_.rows()) -=
      cached_projection_polynomial_derivative_ * point;

  std::vector<double> candidates;
  auto trimmed = trimZeroes(polynomial);
  if (trimmed.size() > 1)
  {
    Eigen::PolynomialSolver<double, Eigen::Dynamic> poly_solver(trimZeroes(polynomial));
    poly_solver.realRoots(candidates);
  }

  double projection = (point - valueAt(0.0)).norm() < (point - valueAt(1.0)).norm() ? 0.0 : 1.0;
  double min = (point - valueAt(projection)).norm();

  for (auto candidate : candidates)
  {
    if (candidate < 0 || candidate > 1)
      continue;

    double dist = (point - valueAt(candidate)).norm();
    if (dist < min)
    {
      projection = candidate;
      min = dist;
    }
  }
  return projection;
}

std::vector<double> Curve::projectPoint(const PointVector& point_vector) const
{
  std::vector<double> t_vector(point_vector.size());
  std::transform(point_vector.begin(), point_vector.end(), t_vector.begin(),
                 [this](const Point& point) { return projectPoint(point); });
  return t_vector;
}

double Curve::distance(const Point& point) const { return (point - valueAt(projectPoint(point))).norm(); }

std::vector<double> Curve::distance(const PointVector& point_vector) const
{
  std::vector<double> dist_vector(point_vector.size());
  std::transform(point_vector.begin(), point_vector.end(), dist_vector.begin(),
                 [this](const Point& point) { return distance(point); });
  return dist_vector;
}

void Curve::applyContinuity(const Curve& source_curve, const std::vector<double>& beta_coeffs)
{
  uint c_order = static_cast<uint>(beta_coeffs.size());

  Eigen::MatrixXd pascal_alterating_matrix(Eigen::MatrixXd::Zero(c_order + 1, c_order + 1));
  pascal_alterating_matrix.diagonal(-1) = -Eigen::ArrayXd::LinSpaced(c_order, 1, c_order);
  pascal_alterating_matrix = pascal_alterating_matrix.exp();

  Eigen::MatrixXd bell_matrix(Eigen::MatrixXd::Zero(c_order + 1, c_order + 1));
  bell_matrix(0, c_order) = 1;
  for (uint i = 0; i < c_order; i++)
    bell_matrix.block(1, c_order - i - 1, i + 1, 1) =
        bell_matrix.block(0, c_order - i, i + 1, i + 1) *
        pascal_alterating_matrix.block(i, 0, 1, i + 1)
            .cwiseAbs()
            .transpose()
            .cwiseProduct(Eigen::Map<const Eigen::MatrixXd>(beta_coeffs.data(), i + 1, 1));

  Eigen::MatrixXd factorial_matrix(Eigen::MatrixXd::Zero(c_order + 1, c_order + 1));
  for (uint i = 0; i < c_order + 1; i++)
    factorial_matrix(i, i) = factorial(N_ - 1) / factorial(N_ - 1 - i);

  Eigen::MatrixXd derivatives;
  derivatives.resize(2, c_order + 1);
  derivatives.col(0) = source_curve.control_points_.bottomRows(1).transpose();
  for (uint i = 1; i < c_order + 1; i++)
    derivatives.col(i) = source_curve.derivative(i).control_points_.bottomRows(1).transpose();

  Eigen::MatrixXd derivatives_wanted = (derivatives * bell_matrix).rowwise().reverse().transpose();

  control_points_.topRows(c_order + 1).noalias() =
      (factorial_matrix * pascal_alterating_matrix).inverse() * derivatives_wanted;
  resetCache();
}

void Curve::resetCache()
{
  N_ = static_cast<uint>(control_points_.rows());
  cached_derivative_.reset();
  cached_roots_.reset();
  cached_bounding_box_.reset();
  cached_polyline_.reset();
  cached_projection_polynomial_part_.reset();
}

Curve::CoeffsMap Curve::bernstein_coeffs_ = CoeffsMap();
Curve::CoeffsMap Curve::splitting_coeffs_left_ = CoeffsMap();
Curve::CoeffsMap Curve::splitting_coeffs_right_ = CoeffsMap();
Curve::CoeffsMap Curve::elevate_order_coeffs_ = CoeffsMap();
Curve::CoeffsMap Curve::lower_order_coeffs_ = CoeffsMap();

Curve::Coeffs Curve::bernsteinCoeffs(uint n)
{
  if (bernstein_coeffs_.find(n) == bernstein_coeffs_.end())
  {
    bernstein_coeffs_.insert({n, Coeffs::Zero(n, n)});
    bernstein_coeffs_[n].diagonal(-1) = -Eigen::ArrayXd::LinSpaced(n - 1, 1, n - 1);
    bernstein_coeffs_[n] = bernstein_coeffs_[n].exp();
    for (uint k = 0; k < n; k++)
      bernstein_coeffs_[n].row(k) *= binomial(n - 1, k);
  }
  return bernstein_coeffs_[n];
}

Curve::Coeffs Curve::splittingCoeffsLeft(uint n, double z)
{
  if (z == 0.5)
  {
    if (splitting_coeffs_left_.find(n) == splitting_coeffs_left_.end())
    {
      splitting_coeffs_left_.insert({n, Coeffs::Zero(n, n)});
      splitting_coeffs_left_[n].diagonal() = Eigen::pow(0.5, Eigen::ArrayXd::LinSpaced(n, 0, n - 1));
      splitting_coeffs_left_[n] = bernsteinCoeffs(n).inverse() * splitting_coeffs_left_[n] * bernsteinCoeffs(n);
    }
    return splitting_coeffs_left_[n];
  }

  Curve::Coeffs coeffs(Coeffs::Zero(n, n));
  coeffs.diagonal() = Eigen::pow(z, Eigen::ArrayXd::LinSpaced(n, 0, n - 1));
  coeffs = bernsteinCoeffs(n).inverse() * coeffs * bernsteinCoeffs(n);
  return coeffs;
}

Curve::Coeffs Curve::splittingCoeffsRight(uint n, double z)
{
  if (z == 0.5)
  {
    if (splitting_coeffs_right_.find(n) == splitting_coeffs_right_.end())
    {
      splitting_coeffs_right_.insert({n, Coeffs::Zero(n, n)});
      Curve::Coeffs temp_splitting_coeffs_left = splittingCoeffsLeft(n);
      for (uint k = 0; k < n; k++)
        splitting_coeffs_right_[n].block(k, k, 1, n - k) = temp_splitting_coeffs_left.block(n - 1 - k, 0, 1, n - k);
    }
    return splitting_coeffs_right_[n];
  }

  Curve::Coeffs coeffs(Coeffs::Zero(n, n));
  Curve::Coeffs temp_splitting_coeffs_left = splittingCoeffsLeft(n, z);
  for (uint k = 0; k < n; k++)
    coeffs.block(k, k, 1, n - k) = temp_splitting_coeffs_left.block(n - 1 - k, 0, 1, n - k);
  return coeffs;
}

Curve::Coeffs Curve::elevateOrderCoeffs(uint n)
{
  if (elevate_order_coeffs_.find(n) == elevate_order_coeffs_.end())
  {
    elevate_order_coeffs_.insert({n, Coeffs::Zero(n + 1, n)});
    elevate_order_coeffs_[n].diagonal() = 1 - Eigen::ArrayXd::LinSpaced(n, 0, n - 1) / n;
    elevate_order_coeffs_[n].diagonal(-1) = Eigen::ArrayXd::LinSpaced(n, 1, n) / n;
  }
  return elevate_order_coeffs_[n];
}

Curve::Coeffs Curve::lowerOrderCoeffs(uint n)
{
  if (lower_order_coeffs_.find(n) == lower_order_coeffs_.end())
  {
    lower_order_coeffs_.insert({n, Coeffs::Zero(n - 1, n)});
    lower_order_coeffs_[n].noalias() = (elevateOrderCoeffs(n - 1).transpose() * elevateOrderCoeffs(n - 1)).inverse() *
                                       elevateOrderCoeffs(n - 1).transpose();
  }
  return lower_order_coeffs_[n];
}
