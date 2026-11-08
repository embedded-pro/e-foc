#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class FrictionFeedForward
    {
    public:
        // Mechanical rad/s: about one encoder count of speed at the 1 kHz outer loop for a 4000-count encoder.
        static constexpr float rampWidth{ 2.0f };

        void Load(NewtonMeter coulomb, NewtonMeter torqueConstant, Ampere ceiling)
        {
            const auto current = coulomb.Value() / torqueConstant.Value();
            const bool plausible = IsFiniteValue(current) && torqueConstant.Value() > 0.0f && current >= 0.0f && current < ceiling.Value();
            amplitude = plausible ? current : 0.0f;
        }

        void Clear()
        {
            amplitude = 0.0f;
        }

        OPTIMIZE_FOR_SPEED float CurrentFor(float speedReference) const
        {
            constexpr float perRadianPerSecond{ 1.0f / rampWidth };
            const auto sign = speedReference * perRadianPerSecond;
            const auto ramped = sign > 1.0f ? 1.0f : (sign < -1.0f ? -1.0f : sign);

            return amplitude * ramped;
        }

    private:
        float amplitude{ 0.0f };
    };
}
