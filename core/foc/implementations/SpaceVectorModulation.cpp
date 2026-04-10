#include "core/foc/implementations/SpaceVectorModulation.hpp"
#include <algorithm>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    SpaceVectorModulation::Output SpaceVectorModulation::Generate(const TwoPhase& voltagePhase) const
    {
        const float alpha = voltagePhase.alpha;
        const float beta = voltagePhase.beta;

        const float alpha_half = alpha * half;
        const float beta_sqrt3 = beta * sqrt3Div2;

        auto vA = alpha;
        auto vB = -alpha_half + beta_sqrt3;
        auto vC = -alpha_half - beta_sqrt3;

        auto vMax = std::max({ vA, vB, vC });
        auto vMin = std::min({ vA, vB, vC });
        auto vCommon = (vMax + vMin) * -half;

        return Output{ Clip(vA + vCommon), Clip(vB + vCommon), Clip(vC + vCommon) };
    }

    float SpaceVectorModulation::Clip(float dutyCycle) const
    {
        return std::clamp(dutyCycle * invSqrt3 + half, zero, one);
    }
}
