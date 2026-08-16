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

        OPTIMIZE_FOR_SPEED foc::RotatingFrame Compute(const CurrentControlContext& context);

    private:
        PidCurrentController pid;
        float couplingScale{ 0.0f };
        float backEmfScale{ 0.0f };
    };

    static_assert(CurrentController<DecoupledPidCurrentController>);
}
