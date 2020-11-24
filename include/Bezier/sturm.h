#ifndef STURM_H
#define STURM_H

#include <Eigen/Dense>
#include <vector>

namespace Sturm
{

/*!
 * \brief Local shape of function at root
 */
enum RootTypeFlag
{
  Convex = 1,     /*! Find all convex roots */
  Concave = 2,    /*! Find all concave roots */
  Inflection = 4, /*! Find all roots at inflections */
  All = 8         /*! Find all roots (overpowers other flags) */
};

/*!
 * \brief Generate Sturm chain
 * \param polynomial Coefficients of polynomial equation (highest degree first)
 * \param epsilon Precision
 * \return A sturm chain generated from polynomial
 */
inline Eigen::MatrixXd chain(const Eigen::VectorXd& polynomial, double epsilon = 0.001)
{
  Eigen::MatrixXd sturm_chain = Eigen::MatrixXd::Zero(polynomial.size(), polynomial.size() + 2);

  sturm_chain.row(0).head(polynomial.size()) = polynomial;
  for (uint j = 1; j < polynomial.size(); j++)
    sturm_chain(1, j) = (polynomial.size() - j) * sturm_chain(0, j - 1);

  for (uint i = 2; i < polynomial.size(); i++)
  {
    const Eigen::VectorXd& d2 = sturm_chain.row(i - 2).tail(polynomial.size() + 2 - i + 2);
    const Eigen::VectorXd& d1 = sturm_chain.row(i - 1).tail(polynomial.size() + 2 - i + 1);

    if (std::fabs(d1.norm() - std::fabs(d1(d1.size() - 3))) < epsilon)
      return sturm_chain.block(0, 0, i, polynomial.size());

    if (std::fabs(d1(0)) > epsilon)
    {
      double T, M;
      T = d2(0) / d1(0);
      M = (d2(1) - T * d1(1)) / d1(0);
      for (uint j = 0; j < polynomial.size() - i; j++)
        sturm_chain(i, i + j) = -(d2(j + 2) - M * d1(j + 1) - T * d1(j + 2));
    }
    else
    {
      auto trim = [](const Eigen::VectorXd p) {
        uint k = 0;
        while (p(k) == 0.0)
          k++;
        return p.tail(p.size() - k);
      };
      auto inflate = [polynomial](const Eigen::VectorXd p) {
        Eigen::VectorXd v = Eigen::VectorXd::Zero(polynomial.size() + 2);
        v.segment(polynomial.size() - p.size(), p.size()) = p;
        return v;
      };

      Eigen::VectorXd a = trim(d2.head(d2.size() - 2));
      Eigen::VectorXd b = trim(d1.head(d1.size() - 2));

      Eigen::VectorXd r = a;
      while (r.size() && r.size() >= b.size())
      {
        double L = r(0) / b(0);
        uint X = static_cast<uint>(r.size() - b.size());

        for (uint k = 0; k < b.size(); k++)
          r(r.size() - (X + b.size()) + k) -= L * b(k);
        r = trim(r);
      }
      sturm_chain.row(i) = inflate(r);
    }
  }
  return sturm_chain.leftCols(polynomial.size());
};

/*!
 * \brief Counts the number of roots in a given interval
 * \param sturm_chain Series of Sturm functions
 * \param t1 start of interval
 * \param t2 end of interval
 * \return Number of roots
 */
inline int interval(const Eigen::MatrixXd& sturm_chain, double t1, double t2)
{
  Eigen::VectorXd power_basis_1 =
      Eigen::pow(t1, Eigen::ArrayXd::LinSpaced(sturm_chain.cols(), sturm_chain.cols() - 1, 0));
  Eigen::VectorXd power_basis_2 =
      Eigen::pow(t2, Eigen::ArrayXd::LinSpaced(sturm_chain.cols(), sturm_chain.cols() - 1, 0));

  Eigen::VectorXd signcount1 = sturm_chain * power_basis_1;
  Eigen::VectorXd signcount2 = sturm_chain * power_basis_2;

  int count1{0}, count2{0};
  for (uint k = 1; k < signcount1.size(); k++)
  {
    if (std::signbit(signcount1(k - 1)) != std::signbit(signcount1(k)))
      count1++;
    if (std::signbit(signcount2(k - 1)) != std::signbit(signcount2(k)))
      count2++;
  }
  return count1 - count2;
};

/*!
 * \brief Find all roots in the [0, 1] interval
 * \param polynomial Coefficients of polynomial equation (highest degree first)
 * \param root_type Type of roots to find
 * \param epsilon Precision of resulting roots
 * \return A vector of roots
 */
inline std::vector<double> roots(const Eigen::VectorXd& polynomial, int root_type = RootTypeFlag::All,
                                 double epsilon = 0.001)
{
  auto sturm_chain = chain(polynomial);
  std::vector<std::tuple<double, double, bool>> intervals;
  std::vector<double> roots;

  auto iterate = [&](std::tuple<double, double, bool> item) {
    double a = std::get<0>(item);
    double b = std::get<1>(item);
    bool flag = std::get<2>(item);
    double a_b = (a + b) / 2;
    uint root_num = static_cast<uint>(interval(sturm_chain, std::get<0>(item), std::get<1>(item)));
    if (root_num && a_b - a < epsilon)
      root_num = 1;

    switch (root_num)
    {
    case 0:
      return;
    case 1:
      if (a_b - a < epsilon)
      {
        roots.emplace_back((a + a_b) / 2);
        return;
      }
      else if (root_type != All && !flag)
      {
        Eigen::VectorXd power_basis_a =
            Eigen::pow(a, Eigen::ArrayXd::LinSpaced(sturm_chain.cols(), sturm_chain.cols() - 1, 0)).eval();
        Eigen::VectorXd power_basis_b =
            Eigen::pow(b, Eigen::ArrayXd::LinSpaced(sturm_chain.cols(), sturm_chain.cols() - 1, 0)).eval();
        double g_a = sturm_chain.row(0) * power_basis_a;
        double g_b = sturm_chain.row(0) * power_basis_b;

        if (!(root_type & All))
        {
          if (root_type & Convex && g_a <= 0 && g_b > 0)
            flag = true;
          if (root_type & Concave && g_a > 0 && g_b <= 0)
            flag = true;
          if (root_type & Inflection && ((g_a >= 0 && g_b >= 0) || (g_a <= 0 && g_b <= 0)))
            flag = true;
          if (!flag)
            return;
        }
      }
      [[clang::fallthrough]];
    default:
      intervals.emplace_back(a, a_b, flag);
      intervals.emplace_back(a_b, b, flag);
    }
  };

  iterate({0.0, 1.0, false});

  while (!intervals.empty())
  {
    double a, b, a_b;
    bool flag;
    std::tie(a, b, flag) = intervals.back();
    intervals.pop_back();
    a_b = (a + b) / 2;

    iterate({a, a_b, flag});
    iterate({a_b, b, flag});
  }

  return roots;
}
} // namespace Sturm
#endif // STURM_H
