#pragma once

#include "core/services/current_controllers/PidCurrentController.hpp"

namespace services
{
    class DecoupledPidCurrentController
    {
    public:
        static constexpr CurrentAlgorithm algorithm{ CurrentAlgorithm::decoupledPid };

        void Configure(const MotorModelParameters& motorParameters);
        void SetTunings(const CurrentControllerTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::RotatingFrame Compute(const CurrentControlContext& context);

    private:
        PidCurrentController pid;
        float couplingScale{ 0.0f };
        float backEmfScale{ 0.0f };
    };

    static_assert(CurrentController<DecoupledPidCurrentController>);
}
