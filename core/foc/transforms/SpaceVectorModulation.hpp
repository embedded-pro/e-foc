#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <cstdint>
#include <numbers>

namespace foc
{
    ALWAYS_INLINE_HOT PhasePwmDutyCycles ToDutyCycles(const ThreePhase& normalized)
    {
        const auto toDutyCycle = [](float value)
        {
            const auto bounded = value > 0.0f ? (value < 1.0f ? value : 1.0f) : 0.0f;

            return hal::DutyCycle{ static_cast<uint32_t>(bounded * static_cast<float>(hal::DutyCycle::fullScale) + 0.5f) };
        };

        return PhasePwmDutyCycles{ toDutyCycle(normalized.a), toDutyCycle(normalized.b), toDutyCycle(normalized.c) };
    }

    class SpaceVectorModulation
    {
    public:
        OPTIMIZE_FOR_SPEED
        PhasePwmDutyCycles Generate(const TwoPhase& voltagePhase) const
        {
            const float alphaHalf = voltagePhase.alpha * half;
            const float betaSqrt3 = voltagePhase.beta * sqrt3Div2;

            const auto vA = voltagePhase.alpha;
            const auto vB = -alphaHalf + betaSqrt3;
            const auto vC = -alphaHalf - betaSqrt3;

            const auto vMax = std::max({ vA, vB, vC });
            const auto vMin = std::min({ vA, vB, vC });
            const auto vCommon = (vMax + vMin) * -half;

            return ToDutyCycles(ThreePhase{ Normalized(vA + vCommon), Normalized(vB + vCommon), Normalized(vC + vCommon) });
        }

    private:
        OPTIMIZE_FOR_SPEED
        float Normalized(float phaseVoltage) const
        {
            return phaseVoltage * invSqrt3 + half;
        }

        static constexpr float half{ 0.5f };
        static constexpr float invSqrt3{ std::numbers::inv_sqrt3_v<float> };
        static constexpr float sqrt3Div2{ std::numbers::sqrt3_v<float> * 0.5f };
    };
}
