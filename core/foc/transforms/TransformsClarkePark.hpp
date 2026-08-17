#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <numbers>

namespace foc
{
    // Bodies live in the header so OPTIMIZE_FOR_SPEED's always_inline can apply at the
    // 20 kHz call sites; the project builds without LTO, so a .cpp definition cannot inline.
    class Clarke
    {
    public:
        OPTIMIZE_FOR_SPEED
        TwoPhase Forward(const ThreePhase& input) const
        {
            const float bcSum = input.b + input.c;
            return TwoPhase{ twoThirds * (input.a - oneHalf * bcSum), invSqrt3 * (input.b - input.c) };
        }

        OPTIMIZE_FOR_SPEED
        ThreePhase Inverse(const TwoPhase& input) const
        {
            const float alphaHalf = oneHalf * input.alpha;
            const float betaSqrt3Half = sqrt3Div2 * input.beta;
            return ThreePhase{ input.alpha, -alphaHalf + betaSqrt3Half, -alphaHalf - betaSqrt3Half };
        }

    private:
        constexpr static float oneHalf = 0.5f;
        constexpr static float twoThirds = 2.0f / 3.0f;
        constexpr static float invSqrt3 = std::numbers::inv_sqrt3_v<float>;
        constexpr static float sqrt3Div2 = std::numbers::sqrt3_v<float> * 0.5f;
    };

    class Park
    {
    public:
        OPTIMIZE_FOR_SPEED
        RotatingFrame Forward(const TwoPhase& input, const float& cosTheta, const float& sinTheta) const
        {
            return RotatingFrame{ input.alpha * cosTheta + input.beta * sinTheta, -input.alpha * sinTheta + input.beta * cosTheta };
        }

        OPTIMIZE_FOR_SPEED
        TwoPhase Inverse(const RotatingFrame& input, const float& cosTheta, const float& sinTheta) const
        {
            return TwoPhase{ input.d * cosTheta - input.q * sinTheta, input.d * sinTheta + input.q * cosTheta };
        }
    };

    class ClarkePark
    {
    public:
        OPTIMIZE_FOR_SPEED
        RotatingFrame Forward(const ThreePhase& input, const float& cosTheta, const float& sinTheta) const
        {
            return park.Forward(clarke.Forward(input), cosTheta, sinTheta);
        }

        OPTIMIZE_FOR_SPEED
        ThreePhase Inverse(const RotatingFrame& input, const float& cosTheta, const float& sinTheta) const
        {
            return clarke.Inverse(park.Inverse(input, cosTheta, sinTheta));
        }

    private:
        [[no_unique_address]] Clarke clarke;
        [[no_unique_address]] Park park;
    };
}
