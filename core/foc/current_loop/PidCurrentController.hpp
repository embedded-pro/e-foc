#pragma once

#include "core/foc/current_loop/CurrentController.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class PidCurrentController
    {
    public:
        static constexpr CurrentAlgorithm algorithm{ CurrentAlgorithm::pid };

        void Configure(const MotorModelParameters& motorParameters);
        void SetTunings(const CurrentLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::RotatingFrame Compute(const CurrentControlContext& context);

    private:
        void ApplyGains();

        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        MotorModelParameters parameters{};
        float bandwidth{ CurrentLoopTunings{}.bandwidth };
    };

    static_assert(CurrentController<PidCurrentController>);
}
