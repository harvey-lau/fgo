/**
 *
 *
 */

#include "AnalyStats.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace FGo
{
namespace Analy
{

namespace StatsUtils
{

double calcDerivative(double xx, std::function<double(double)> func)
{
    double step = 1e-9;
    double delta = func(xx + step) - func(xx - step);
    if (delta == 0.0) return DBL_MAX;
    else return delta / (2 * step);
}

long double calcDerivative(long double xx, std::function<long double(long double)> func)
{
    long double step = 1e-12L;
    long double delta = func(xx + step) - func(xx - step);
    if (delta == 0.0L) return LDBL_MAX;
    else return delta / (2 * step);
}

double funcGamma(double num)
{
    return std::tgamma(num);
}

long double funcGamma(long double num)
{
    return std::tgammal(num);
}

double funcLogGamma(double num)
{
    return std::lgamma(num);
}

long double funcLogGamma(long double num)
{
    return std::lgammal(num);
}

double funcDigamma(double num)
{
    bool EulerFlag = true;

    // Euler's product formula
    double result = 0.0 - EulerMascheroniConst;
    for (uint32_t i = 0; i < INT32_MAX; ++i) {
        double nPlusZ = num + (double)i;
        if (nPlusZ == 0.0) {
            EulerFlag = false;
            break;
        }
        double delta = (num - 1) / ((double)(i + 1) * nPlusZ);
        result += delta;
        if (delta <= 1e-14) break;
    }

    if (EulerFlag) return result;
    else {
        // `num`=0, -1, -2 ...
        return calcDerivative(num, [](double xx) {
            return funcLogGamma(xx);
        });
    }
}

long double funcDigamma(long double num)
{
    bool EulerFlag = true;

    // Euler's product formula
    long double result = 0.0L - EulerMascheroniLongConst;
    for (uint32_t i = 0; i < INT32_MAX; ++i) {
        long double nPlusZ = num + (long double)i;
        if (nPlusZ == 0.0L) {
            EulerFlag = false;
            break;
        }
        long double delta = (num - 1) / ((long double)(i + 1) * nPlusZ);
        result += delta;
        if (delta <= 1e-14L) break;
    }

    if (EulerFlag) return result;
    else {
        // `num`=0, -1, -2 ...
        return calcDerivative(num, [](long double xx) {
            return funcLogGamma(xx);
        });
    }
}

double func2Digamma(double num)
{
    return calcDerivative(num, [](double xx) {
        return funcDigamma(xx);
    });
}

long double func2Digamma(long double num)
{
    return calcDerivative(num, [](long double xx) {
        return funcDigamma(xx);
    });
}

double funcLowerIncompleteGamma(double s, double x)
{
    double gammaS = funcGamma(s);
    if (std::isinf(gammaS)) return gammaS;
    double prevProduct = std::pow(x, s) * gammaS * std::exp(0.0 - x);
    double postPlus = 0.0;
    double delta = DBL_MAX;
    double minDelta = 1e-9;
    for (uint32_t i = 0; i < INT32_MAX; ++i) {
        double tmpGammaRes = funcGamma(s + (double)(i + 1));
        if (std::isinf(tmpGammaRes)) continue;
        delta = std::pow(x, i) / tmpGammaRes;
        postPlus += delta;
        if (delta <= minDelta) break;
    }
    return prevProduct * postPlus;
}

long double funcLowerIncompleteGamma(long double s, long double x)
{
    long double gammaS = funcGamma(s);
    if (std::isinf(gammaS)) return gammaS;
    long double prevProduct = std::pow(x, s) * gammaS * std::exp(0.0L - x);
    long double postPlus = 0.0L;
    long double delta = LDBL_MAX;
    long double minDelta = 1e-9L;
    for (uint32_t i = 0; i < INT32_MAX; ++i) {
        long double tmpGammaRes = funcGamma(s + (long double)(i + 1));
        if (std::isinf(tmpGammaRes)) continue;
        delta = std::pow(x, i) / tmpGammaRes;
        postPlus += delta;
        if (delta <= minDelta) break;
    }
    return prevProduct * postPlus;
}

} // namespace StatsUtils

void GammaDistrib::estimate(const Vector<uint32_t> &data, bool usingMLE /*=true*/)
{
    if (data.empty())
        throw UnexpectedException(
            "Sample data set is empty during estimating gamma distribution"
        );

    // Use MLE
    long double avgLogX = 0.0L;
    long double avgX = 0.0L;
    // Use method of moment
    long double expectation = 0.0L;
    long double squareExpect = 0.0L;
    for (auto num : data) {
        if (num <= 0) continue;
        long double cur_num = num;
        avgX += cur_num;
        avgLogX += std::log(cur_num);

        expectation += cur_num;
        squareExpect += cur_num * cur_num;
    }
    avgX /= (long double)data.size();
    avgLogX /= (long double)data.size();
    long double logAvgX = std::log(avgX);

    expectation /= (long double)data.size();
    squareExpect /= (long double)data.size();
    long double expectSquare = expectation * expectation;
    long double variance = squareExpect - expectSquare;

    if (!usingMLE) {
        if (variance == 0.0L)
            throw InvalidDataSetException("The variance of this data set equals to zero");

        m_alpha = expectSquare / variance;
        m_beta = expectation / variance;
    }
    else {

        // We use MLE (Maximum Likelihood Estimation) to estimate the gamma distribution
        // following the iterative methods given by Thomas P. Minka in his paper
        // (https://tminka.github.io/papers/minka-gamma.pdf). For simplicity,
        // you can also use the method of moments to estimate the distribution,
        // and then you only need to calculate the expectation and variance.

        // Calculate starting point
        if (logAvgX < avgLogX)
            throw UnexpectedException("log(avg(x))<avg(log(x)) against Jensen's "
                                      "inequality during estimating gamma distribution");
        if (logAvgX == avgLogX)
            throw InvalidDataSetException(
                "log(avg(x))=avg(log(x)) the variance of this data set may equal to zero"
            );
        long double startingPoint = 0.5L / (logAvgX - avgLogX);

        long double deltaParamX = avgLogX - logAvgX;
        long double alpha = startingPoint;
        long double delta = LDBL_MAX;
        long double minDelta = 1e-9L;

        // Approximation via generalized Newton
        while (delta > minDelta) {
            delta = (deltaParamX + std::log(alpha) - StatsUtils::funcDigamma(alpha)) /
                    (alpha - alpha * alpha * StatsUtils::func2Digamma(alpha));
            alpha = 1.0L / (1.0L / alpha + delta);
            delta = std::abs(delta);
        }

        m_alpha = alpha;
        m_beta = alpha / avgX;
    }
}

void GammaDistrib::getCDFQuantile(uint32_t start, uint32_t end, Vector<long double> &quantile)
{
    if (m_alpha == 0.0L || m_beta == 0.0L)
        throw UnexpectedException("The estimation haven't been conducted");

    quantile.resize(end - start + 1, 0.0L);
    size_t index = 0;
    if (start == 0) {
        quantile[0] = 0.0L;
        start++;
        index++;
    }

    long double prevGammaAlpha = 1.0L / StatsUtils::funcGamma(m_alpha);
    for (; index < quantile.size(); ++index) {
        quantile[index] =
            prevGammaAlpha *
            StatsUtils::funcLowerIncompleteGamma(m_alpha, m_beta * (long double)(start++));
    }
}
} // namespace Analy
} // namespace FGo