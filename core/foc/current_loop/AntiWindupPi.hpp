#pragma once

#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class AntiWindupPi
    {
    public:
        void SetTunings(float proportional, float integral)
        {
            kp = proportional;
            ki = integral;
        }

        void Reset()
        {
            previousOutput = 0.0f;
            previousError = 0.0f;
        }

        ALWAYS_INLINE_HOT float Propose(float reference, float measured)
        {
            const auto error = reference - measured;
            const auto output = previousOutput + (kp + ki) * error - kp * previousError;

            previousError = error;

            return output;
        }

        ALWAYS_INLINE_HOT void CommitRealized(float applied)
        {
            previousOutput = applied;
        }

    private:
        float kp{ 0.0f };
        float ki{ 0.0f };
        float previousOutput{ 0.0f };
        float previousError{ 0.0f };
    };
}
