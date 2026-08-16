#pragma once

#include "core/foc/current_loop/PidCurrentController.hpp"

namespace foc
{
    class DecoupledPidCurrentController
    {
    public:
        static constexpr CurrentAlgorithm algorithm{ CurrentAlgorithm::decoupledPid };

        void Configure(const MotorModelParameters& motorParameters);
        void SetTunings(const CurrentLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED RotatingFrame Compute(const CurrentControlContext& context)
        {
            return LimitToModulationCircle(decoupling.Apply(pid.Compute(context), context));
        }

    private:
        PidCurrentController pid;
        DecouplingFeedforward decoupling;
    };

    static_assert(CurrentController<DecoupledPidCurrentController>);
}
