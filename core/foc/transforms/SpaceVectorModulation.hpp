#pragma once

#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>

namespace foc
{
    // Body lives in the header so OPTIMIZE_FOR_SPEED's always_inline can apply at the
    // 20 kHz call sites; the project builds without LTO, so a .cpp definition cannot inline.
    class SpaceVectorModulation
    {
    public:
        struct Output
        {
            float a;
            float b;
            float c;
        };

        OPTIMIZE_FOR_SPEED
        Output Generate(const TwoPhase& voltagePhase) const
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

    private:
        OPTIMIZE_FOR_SPEED
        float Clip(float dutyCycle) const
        {
            return std::clamp(dutyCycle * invSqrt3 + half, zero, one);
        }

        static constexpr float zero{ float(0.0f) };
        static constexpr float one{ float(1.0f) };
        static constexpr float half{ float(0.5f) };
        static constexpr float invSqrt3{ float(0.577350269189625f) };
        static constexpr float sqrt3Div2{ float(0.866025403784438f) };
    };
}
