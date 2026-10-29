#pragma once

#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class AntiWindupPi
    {
    public:
        void SetTunings(float proportional, float integral)
        {
            errorGain = proportional + integral;
            previousErrorGain = -proportional;
        }

        void Reset()
        {
            output = 0.0f;
            error = 0.0f;
        }

        ALWAYS_INLINE_HOT float Propose(float reference, float measured)
        {
            const auto previousError = error;
            error = reference - measured;
            output = output + errorGain * error + previousErrorGain * previousError;
            return output;
        }

        ALWAYS_INLINE_HOT void CommitRealized(float applied)
        {
            output = applied;
        }

    private:
        float errorGain{ 0.0f };
        float previousErrorGain{ 0.0f };
        float output{ 0.0f };
        float error{ 0.0f };
    };
}
