#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include "core/foc/implementations/TransformsClarkePark.hpp"

namespace foc
{
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
        Output Generate(const TwoPhase& voltagePhase) const;

    private:
        OPTIMIZE_FOR_SPEED
        float Clip(float dutyCycle) const;

        static constexpr float zero{ float(0.0f) };
        static constexpr float one{ float(1.0f) };
        static constexpr float half{ float(0.5f) };
        static constexpr float invSqrt3{ float(0.577350269189625f) };
        static constexpr float sqrt3Div2{ float(0.866025403784438f) };
    };
}
