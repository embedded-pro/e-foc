#pragma once

#include "core/foc/current_loop/CurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"
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

        OPTIMIZE_FOR_SPEED RotatingFrame Compute(const CurrentControlContext& context)
        {
            dPid.SetPoint(context.reference.d);
            qPid.SetPoint(context.reference.q);

            return LimitToModulationCircle({ dPid.Process(context.measured.d), qPid.Process(context.measured.q) });
        }

    private:
        void ApplyGains();

        controllers::PidIncrementalSynchronous<float> dPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        controllers::PidIncrementalSynchronous<float> qPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        MotorModelParameters parameters{};
        float bandwidth{ CurrentLoopTunings{}.bandwidth };
    };

    static_assert(CurrentController<PidCurrentController>);
}
