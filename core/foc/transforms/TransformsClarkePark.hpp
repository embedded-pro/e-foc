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
            const float a = input.a;
            const float b = input.b;
            const float c = input.c;

            const float bc_sum = b + c;
            return TwoPhase{ twoThirds * (a - oneHalf * bc_sum), invSqrt3 * (b - c) };
        }

        OPTIMIZE_FOR_SPEED
        ThreePhase Inverse(const TwoPhase& input) const
        {
            const float alpha = input.alpha;
            const float beta = input.beta;

            const float alpha_half = oneHalf * alpha;
            const float beta_sqrt3_half = sqrt3Div2 * beta;
            return ThreePhase{ alpha, -alpha_half + beta_sqrt3_half, -alpha_half - beta_sqrt3_half };
        }

    private:
        constexpr static float oneHalf = 0.5f;
        constexpr static float twoThirds = 0.666666667f;
        constexpr static float invSqrt3 = std::numbers::inv_sqrt3_v<float>;
        constexpr static float sqrt3Div2 = 0.8660254037f;
    };

    class Park
    {
    public:
        OPTIMIZE_FOR_SPEED
        RotatingFrame Forward(const TwoPhase& input, const float& cosTheta, const float& sinTheta) const
        {
            const float alpha = input.alpha;
            const float beta = input.beta;
            const float cos_t = cosTheta;
            const float sin_t = sinTheta;

            const float alpha_cos = alpha * cos_t;
            const float beta_sin = beta * sin_t;
            const float alpha_sin = alpha * sin_t;
            const float beta_cos = beta * cos_t;

            return RotatingFrame{ alpha_cos + beta_sin, -alpha_sin + beta_cos };
        }

        OPTIMIZE_FOR_SPEED
        TwoPhase Inverse(const RotatingFrame& input, const float& cosTheta, const float& sinTheta) const
        {
            const float d = input.d;
            const float q = input.q;
            const float cos_t = cosTheta;
            const float sin_t = sinTheta;

            const float d_cos = d * cos_t;
            const float q_sin = q * sin_t;
            const float d_sin = d * sin_t;
            const float q_cos = q * cos_t;

            return TwoPhase{ d_cos - q_sin, d_sin + q_cos };
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
