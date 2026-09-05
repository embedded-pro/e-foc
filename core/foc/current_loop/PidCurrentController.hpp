#pragma once

#include "core/foc/current_loop/AntiWindupPi.hpp"
#include "core/foc/current_loop/CurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"
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
            const auto applied = LimitToModulationCircle(Propose(context));

            CommitRealized(applied);

            return applied;
        }

        ALWAYS_INLINE_HOT RotatingFrame Propose(const CurrentControlContext& context)
        {
            return { dPi.Propose(context.reference.d, context.measured.d),
                qPi.Propose(context.reference.q, context.measured.q) };
        }

        ALWAYS_INLINE_HOT void CommitRealized(const RotatingFrame& applied)
        {
            dPi.CommitRealized(applied.d);
            qPi.CommitRealized(applied.q);
        }

    private:
        void ApplyGains();

        AntiWindupPi dPi;
        AntiWindupPi qPi;
        MotorModelParameters parameters{};
        float bandwidth{ CurrentLoopTunings{}.bandwidth };
    };

    static_assert(CurrentController<PidCurrentController>);
}
