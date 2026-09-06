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
            const auto proposed = pid.Propose(context);
            const auto total = decoupling.Apply(proposed, context);
            const auto applied = LimitToModulationCircle(total);

            pid.CommitRealized({ proposed.d + (applied.d - total.d), proposed.q + (applied.q - total.q) });

            return applied;
        }

    private:
        PidCurrentController pid;
        DecouplingFeedforward decoupling;
    };

    static_assert(CurrentController<DecoupledPidCurrentController>);
}
