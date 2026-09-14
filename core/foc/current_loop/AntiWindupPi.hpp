#pragma once

#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "numerical/controllers/interfaces/PidController.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <limits>

namespace foc
{
    class AntiWindupPi
    {
    public:
        void SetTunings(float proportional, float integral)
        {
            pid.SetTunings({ proportional, integral, 0.0f });
        }

        void Reset()
        {
            pid.Reset();
        }

        ALWAYS_INLINE_HOT float Propose(float reference, float measured)
        {
            pid.SetPoint(reference);
            return pid.Process(measured);
        }

        ALWAYS_INLINE_HOT void CommitRealized(float applied)
        {
            pid.SetPreviousOutput(applied);
        }

    private:
        controllers::PidIncrementalSynchronous<float> pid{
            { 0.0f, 0.0f, 0.0f },
            { -std::numeric_limits<float>::max(), std::numeric_limits<float>::max() }
        };
    };
}
