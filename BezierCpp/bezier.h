/*
 * Copyright 2019 Mirko Kokot
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BEZIER_H
#define BEZIER_H

#include <Eigen/Dense>
#include <vector>
#include <map>
#include <memory>

/*!
 * Nominal namespace containing class definition and typedefs
 */
namespace Bezier
{
/*!
 * \brief Point in xy plane
 */
typedef Eigen::Vector2d Point;
/*!
 * \brief A Vector in xy plane
 */
typedef Eigen::Vector2d Vec2;
/*!
 * \brief A vector of Points
 */
typedef std::vector<Point> PointVector;
/*!
 * \brief Bounding box
 */
typedef Eigen::AlignedBox2d BBox;

/*!
 * \brief A Bezier curve class
 *
 * A class for storing and using any-order Bezier curve.
 * It uses private and static caching for storing often accessed data.
 * Private caching is used for data concerning individual curve, while
 * static caching is used for common data (coefficient matrices)
 */
class Curve : public std::enable_shared_from_this<Curve>
{
private:
  /*!
   * \brief Coefficients for matrix operations
   */
  typedef Eigen::MatrixXd Coeffs;
  /*!
   * \brief Map of different coefficient matrices, depending on the order of the curve
   */
  typedef std::map<uint, Coeffs> CoeffsMap;

  /// Number of control points (order + 1)
  uint N_;
  /// N x 2 matrix where each row corresponds to control Point
  Eigen::MatrixX2d control_points_;

  // private caching
  std::shared_ptr<Curve> cached_derivative_;              /*! If generated, stores derivative for later use */
  std::shared_ptr<std::vector<Point>> cached_ext_points_; /*! If generated, stores extreme Points for later use */
  std::shared_ptr<BBox>
      cached_bounding_box_tight_; /*! If generated, stores bounding box (use_roots = true) for later use */
  std::shared_ptr<BBox>
      cached_bounding_box_relaxed_; /*! If generated, stores bounding box (use_roots = false) for later use */
  std::shared_ptr<PointVector> cached_polyline_; /*! If generated, stores polyline for later use */
  double cached_polyline_smootheness_ = 0;       /*! Smootheness of cached polyline */

  /// Reset all privately cached data
  inline void resetCache();

  // static caching
  static CoeffsMap bernstein_coeffs_;       /*! Map of Bernstein coefficients */
  static CoeffsMap splitting_coeffs_left_;  /*! Map of coefficients to get subcurve for t = [0, 0.5] */
  static CoeffsMap splitting_coeffs_right_; /*! Map of coefficients to get subcurve for t = [0.5, 1] */
  static CoeffsMap elevate_order_coeffs_;   /*! Map of coefficients for elevating the order of curve */
  static CoeffsMap lower_order_coeffs_;     /*! Map of coefficients for lowering the order of curve */

  /// Private getter function for Bernstein coefficients
  Coeffs bernsteinCoeffs() const;
  /// Private getter function for coefficients to get a subcurve t = [0, z];
  Coeffs splittingCoeffsLeft(double z = 0.5) const;
  /// Private getter function for coefficients to get a subcurve t = [z, 1];
  Coeffs splittingCoeffsRight(double z = 0.5) const;
  /// Private getter function for coefficients to elevate order of curve
  Coeffs elevateOrderCoeffs(uint n) const;
  /// Private getter function for coefficients to lower order of curve
  Coeffs lowerOrderCoeffs(uint n) const;

public:
  /*!
   * \brief Create the Bezier curve
   * \param points Nx2 matrix where each row is one of N control points that define the curve
   */
  Curve(const Eigen::MatrixX2d& points);

  /*!
   * \brief Create the Bezier curve
   * \param points A vector of control points that define the curve
   */
  Curve(const PointVector& points);

  /*!
   * \brief Get the control points
   * \return A vector of control points
   */
  PointVector getControlPoints() const;

  /*!
   * \brief Get a polyline representation of curve as a vector of points on curve
   * \param smoothness Smoothness factor > 1 (more resulting points when closer to 1)
   * \param precision Minimal distance between two subsequent points
   * \return A vector of polyline vertices
   */
  PointVector getPolyline(double smoothness = 1.0001, double precision = 1.0) const;

  /*!
   * \brief Set the new coordinates to a control point
   * \param index Index of chosen control point
   * \param point New control point
   */
  void manipulateControlPoint(uint index, const Point& point);

  /*!
   * \brief Manipulate the curve so that it passes through wanted point for given 't'
   * \param t Curve parameter
   * \param point Point where curve should pass for a given t
   *
   * Only works for quadratic and cubic curves
   * \warning Resets cached data
   */
  void manipulateCurvature(double t, const Point& point);

  /*!
   * \brief Raise the curve order by 1
   *
   * Curve will always retain its shape
   * \warning Resets cached data
   */
  void elevateOrder();

  /*!
   * \brief Lower the curve order by 1
   *
   * If current shape cannot be described by lower order, it will be best aproximation
   * \warning Resets cached data
   */
  void lowerOrder();

  /*!
   * \brief Get the point on curve for a given t
   * \param t Curve parameter
   * \return Point on a curve for a given t
   */
  Point valueAt(double t) const;

  /*!
   * \brief Get curvature of curve for a given t
   * \param t Curve parameter
   * \return Curvature of a curve for a given t
   */
  double curvatureAt(double t) const;

  /*!
   * \brief Get the tangent of curve for a given t
   * \param t Curve parameter
   * \param normalize If the resulting tangent should be normalized
   * \return Tangent of a curve for a given t
   */
  Vec2 tangentAt(double t, bool normalize = true) const;

  /*!
   * \brief Get the normal of curve for a given t
   * \param t Curve parameter
   * \param normalize If the resulting normal should be normalized
   * \return Normal of a curve for given t
   */
  Vec2 normalAt(double t, bool normalize = true) const;

  /*!
   * \brief Get the derivative of a curve
   * \return Derivative curve
   */
  Curve getDerivative() const;

  /*!
   * \brief Get the extreme points of curve
   * \param step Size of step in coarse search
   * \param epsilon Precision of resulting t
   * \param max_iter Maximum number of iterations for Newton-Rhapson
   * \return A vector of extreme points
   */
  std::vector<Point> getRoots(double step = 0.1, double epsilon = 0.001, std::size_t max_iter = 15) const;

  /*!
   * \brief Get the bounding box of curve
   * \param use_roots If algorithm should use extreme points
   * \return Bounding box (if use_roots is false, returns the bounding box of control points)
   */
  BBox getBBox(bool use_roots = true) const;

  /*!
   * \brief Split the curve into two subcurves
   * \param z Parameter t at which to split the curve
   * \return Pair of two subcurves
   */
  std::pair<Curve, Curve> splitCurve(double z = 0.5) const;

  /*!
   * \brief Get the points of intersection with another curve
   * \param curve Curve to intersect with
   * \param stop_at_first If first point of intersection is enough
   * \param epsilon Precision of resulting intersection
   * \return A vector af points of intersection between curves
   */
  std::vector<Point> getPointsOfIntersection(const Curve& curve, bool stop_at_first = false,
                                             double epsilon = 0.001) const;

  /*!
   * \brief Get the parameter t where curve is closes to given point
   * \param point Point to project on curve
   * \param step Size of step in coarse search
   * \return Parameter t
   *
   * \warning self-intersection not yet implemented
   */
  double projectPointOnCurve(const Point& point, double step = 0.01) const;
};
}

#endif // BEZIER_H
