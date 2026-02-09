#include "source/foc/implementations/TransformsClarkePark.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    TwoPhase Clarke::Forward(const ThreePhase& input) const
    {
        const float a = input.a;
        const float b = input.b;
        const float c = input.c;

        const float bc_sum = b + c;
        return TwoPhase{ twoThirds * (a - oneHalf * bc_sum), invSqrt3 * (b - c) };
    }

    ThreePhase Clarke::Inverse(const TwoPhase& input) const
    {
        const float alpha = input.alpha;
        const float beta = input.beta;

        const float alpha_half = oneHalf * alpha;
        const float beta_sqrt3_half = sqrt3Div2 * beta;
        return ThreePhase{ alpha, -alpha_half + beta_sqrt3_half, -alpha_half - beta_sqrt3_half };
    }

    RotatingFrame Park::Forward(const TwoPhase& input, const float& cosTheta, const float& sinTheta) const
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

    TwoPhase Park::Inverse(const RotatingFrame& input, const float& cosTheta, const float& sinTheta) const
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

    RotatingFrame ClarkePark::Forward(const ThreePhase& input, const float& cosTheta, const float& sinTheta) const
    {
        return park.Forward(clarke.Forward(input), cosTheta, sinTheta);
    }

    ThreePhase ClarkePark::Inverse(const RotatingFrame& input, const float& cosTheta, const float& sinTheta) const
    {
        return clarke.Inverse(park.Inverse(input, cosTheta, sinTheta));
    }
}
