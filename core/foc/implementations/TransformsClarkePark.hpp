#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <numbers>

namespace foc
{
    struct ThreePhase
    {
        float a;
        float b;
        float c;
    };

    struct TwoPhase
    {
        float alpha;
        float beta;
    };

    struct RotatingFrame
    {
        float d;
        float q;
    };

    class Clarke
    {
    public:
        OPTIMIZE_FOR_SPEED
        TwoPhase Forward(const ThreePhase& input) const;

        OPTIMIZE_FOR_SPEED
        ThreePhase Inverse(const TwoPhase& input) const;

    private:
        constexpr static float oneHalf = float(0.5f);
        constexpr static float twoThirds = float(0.666666667f);
        constexpr static float invSqrt3 = std::numbers::inv_sqrt3_v<float>;
        constexpr static float sqrt3Div2 = float(0.8660254037f);
    };

    class Park
    {
    public:
        OPTIMIZE_FOR_SPEED
        RotatingFrame Forward(const TwoPhase& input, const float& cosTheta, const float& sinTheta) const;

        OPTIMIZE_FOR_SPEED
        TwoPhase Inverse(const RotatingFrame& input, const float& cosTheta, const float& sinTheta) const;
    };

    class ClarkePark
    {
    public:
        OPTIMIZE_FOR_SPEED
        RotatingFrame Forward(const ThreePhase& input, const float& cosTheta, const float& sinTheta) const;

        OPTIMIZE_FOR_SPEED
        ThreePhase Inverse(const RotatingFrame& input, const float& cosTheta, const float& sinTheta) const;

    private:
        [[no_unique_address]] Clarke clarke;
        [[no_unique_address]] Park park;
    };
}
