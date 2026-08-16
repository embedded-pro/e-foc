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
            const auto feedback = pid.Compute(context);
            const auto electricalSpeed = context.electricalSpeed;

            const auto dFeedforward = -electricalSpeed * couplingScale * context.measured.q;
            const auto qFeedforward = electricalSpeed * (couplingScale * context.measured.d + backEmfScale);

            return LimitToModulationCircle({ feedback.d + dFeedforward, feedback.q + qFeedforward });
        }

    private:
        PidCurrentController pid;
        float couplingScale{ 0.0f };
        float backEmfScale{ 0.0f };
    };

    static_assert(CurrentController<DecoupledPidCurrentController>);
}
