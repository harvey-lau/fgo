/**
 *
 *
 */

#ifndef JY_ANALYSTATS_H_
#define JY_ANALYSTATS_H_

#include "AnalyUtils.h"

#include <functional>

namespace FGo
{
namespace Analy
{

namespace StatsUtils
{

const double EulerMascheroniConst = 0.5772156649015329;
const long double EulerMascheroniLongConst = 0.5772156649015329L;

/// @brief Simply calculate the derivative of a function (precision=1e-6)
/// @param xx
/// @param func
/// @return
double calcDerivative(double xx, std::function<double(double)> func);

/// @brief Simply calculate the derivative of a function (precision=1e-9)
/// @param xx
/// @param func
/// @return
long double calcDerivative(long double xx, std::function<long double(long double)> func);

/// @brief Gamma function
/// @param num
/// @return
double funcGamma(double num);

/// @brief Gamma function
/// @param num
/// @return
long double funcGamma(long double num);

/// @brief Calculate the natural logarithm of gamma function
/// @param num
/// @return
double funcLogGamma(double num);

/// @brief Calculate the natural logarithm of gamma function
/// @param num
/// @return
long double funcLogGamma(long double num);

/// @brief Digamma function via Euler's product formula
/// @param num
/// @return
double funcDigamma(double num);

/// @brief Digamma function via Euler's product formula
/// @param num
/// @return
long double funcDigamma(long double num);

/// @brief The derivative of digamma function
/// @param num
/// @return
double func2Digamma(double num);

/// @brief The derivative of digamma function
/// @param num
/// @return
long double func2Digamma(long double num);

/// @brief Lower incomplete gamma function via holomorphic extension
/// @param s
/// @param x
/// @return
double funcLowerIncompleteGamma(double s, double x);

/// @brief Lower incomplete gamma function via holomorphic extension
/// @param s
/// @param x
/// @return
long double funcLowerIncompleteGamma(long double s, long double x);
} // namespace StatsUtils

class BaseDistrib
{
protected:
public:
    BaseDistrib() = default;
    ~BaseDistrib() = default;

    virtual void estimate(const Vector<uint32_t> &data, bool usingMLE = true) = 0;
};

class GammaDistrib : public BaseDistrib
{
public:
    long double m_alpha;
    long double m_beta;

public:
    GammaDistrib() : m_alpha(0.0L), m_beta(0.0L)
    {}
    ~GammaDistrib() = default;

    /// @brief Estimate via MLE
    /// @param data
    /// @exception `AnalyException`
    void estimate(const Vector<uint32_t> &data, bool usingMLE = true);

    /// @brief Get quantile of CDF
    /// @param start
    /// @param end
    /// @param quantile
    void getCDFQuantile(uint32_t start, uint32_t end, Vector<long double> &quantile);
};

} // namespace Analy
} // namespace FGo

#endif