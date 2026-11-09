#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>

namespace foc
{
    class FrictionFeedForward
    {
    public:
        static constexpr float rampWidth{ 2.0f };

        void Load(NewtonMeter coulomb, NewtonMeter torqueConstant, Ampere ceiling)
        {
            const auto current = coulomb.Value() / torqueConstant.Value();
            const bool plausible = math::IsFinite(current) && torqueConstant.Value() > 0.0f && current >= 0.0f && current < ceiling.Value();
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
            const auto ramped = std::clamp(sign, -1.0f, 1.0f);

            return amplitude * ramped;
        }

    private:
        float amplitude{ 0.0f };
    };
}
