#pragma once

#include "core/foc/current_loop/CurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <limits>

namespace foc
{
    class PidCurrentController
    {
    public:
        static constexpr CurrentAlgorithm algorithm{ CurrentAlgorithm::pid };

        bool Configure(const MotorModelParameters& motorParameters);
        bool SetTunings(const CurrentLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED RotatingFrame Compute(const CurrentControlContext& context)
        {
            const auto applied = LimitToModulationCircle(Propose(context), context.busRatio);

            CommitRealized(applied);

            return applied;
        }

        ALWAYS_INLINE_HOT RotatingFrame Propose(const CurrentControlContext& context)
        {
            dPi.SetPoint(context.reference.d);
            qPi.SetPoint(context.reference.q);
            return { dPi.Process(context.measured.d), qPi.Process(context.measured.q) };
        }

        ALWAYS_INLINE_HOT void CommitRealized(const RotatingFrame& applied)
        {
            dPi.SetPreviousOutput(applied.d);
            qPi.SetPreviousOutput(applied.q);
        }

    private:
        bool ApplyGains();

        static constexpr controllers::PidLimits<float> unbounded{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max() };

        controllers::PidIncrementalSynchronous<float> dPi{ { 0.0f, 0.0f, 0.0f }, unbounded };
        controllers::PidIncrementalSynchronous<float> qPi{ { 0.0f, 0.0f, 0.0f }, unbounded };
        MotorModelParameters parameters{};
        float bandwidth{ CurrentLoopTunings{}.bandwidth };
    };

    static_assert(CurrentController<PidCurrentController>);
}
