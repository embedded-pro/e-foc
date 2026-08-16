#pragma once

#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <numbers>

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
            const float alphaHalf = voltagePhase.alpha * half;
            const float betaSqrt3 = voltagePhase.beta * sqrt3Div2;

            const auto vA = voltagePhase.alpha;
            const auto vB = -alphaHalf + betaSqrt3;
            const auto vC = -alphaHalf - betaSqrt3;

            const auto vMax = std::max({ vA, vB, vC });
            const auto vMin = std::min({ vA, vB, vC });
            const auto vCommon = (vMax + vMin) * -half;

            return Output{ Clip(vA + vCommon), Clip(vB + vCommon), Clip(vC + vCommon) };
        }

    private:
        OPTIMIZE_FOR_SPEED
        float Clip(float dutyCycle) const
        {
            return std::clamp(dutyCycle * invSqrt3 + half, zero, one);
        }

        static constexpr float zero{ 0.0f };
        static constexpr float one{ 1.0f };
        static constexpr float half{ 0.5f };
        static constexpr float invSqrt3{ std::numbers::inv_sqrt3_v<float> };
        static constexpr float sqrt3Div2{ std::numbers::sqrt3_v<float> * 0.5f };
    };
}
