#pragma once

#include "core/services/current_controllers/CurrentController.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace services
{
    class PidCurrentController
    {
    public:
        static constexpr CurrentAlgorithm algorithm{ CurrentAlgorithm::pid };

        void Configure(const MotorModelParameters& motorParameters);
        void SetTunings(const CurrentControllerTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::RotatingFrame Compute(const CurrentControlContext& context);

    private:
        void ApplyGains();

        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        MotorModelParameters parameters{};
        float bandwidth{ CurrentControllerTunings{}.bandwidth };
    };

    static_assert(CurrentController<PidCurrentController>);
}
