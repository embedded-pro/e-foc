#pragma once

#include "core/foc/current_loop/CurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class DeadbeatCurrentController
    {
    public:
        static constexpr CurrentAlgorithm algorithm{ CurrentAlgorithm::deadbeat };

        bool Configure(const MotorModelParameters& motorParameters);
        bool SetTunings(const CurrentLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED RotatingFrame Compute(const CurrentControlContext& context)
        {
            EstimateDisturbance(context.measured);

            const auto predicted = delayed ? Predict(context.measured, lastInput) : context.measured;
            const RotatingFrame command{ referenceGain * context.reference.d - feedbackGain * predicted.d - disturbance.d, referenceGain * context.reference.q - feedbackGain * predicted.q - disturbance.q };

            const auto decoupled = decoupling.Apply(command, context);
            const auto applied = LimitToModulationCircle(decoupled);
            const RotatingFrame effective{ command.d + applied.d - decoupled.d, command.q + applied.q - decoupled.q };

            expected = delayed ? predicted : Predict(context.measured, effective);
            lastInput = effective;

            return applied;
        }

    private:
        bool ApplyGains();

        ALWAYS_INLINE_HOT void EstimateDisturbance(const RotatingFrame& measured)
        {
            if (primed)
            {
                disturbance.d += disturbanceGain * (measured.d - expected.d);
                disturbance.q += disturbanceGain * (measured.q - expected.q);
            }

            primed = true;
        }

        ALWAYS_INLINE_HOT RotatingFrame Predict(const RotatingFrame& measured, const RotatingFrame& input) const
        {
            return { plantStateGain * measured.d + plantInputGain * (input.d + disturbance.d), plantStateGain * measured.q + plantInputGain * (input.q + disturbance.q) };
        }

        MotorModelParameters parameters{};
        DecouplingFeedforward decoupling;
        bool twoStep{ CurrentLoopTunings{}.twoStepDeadbeat };
        float bandwidth{ CurrentLoopTunings{}.bandwidth };
        float referenceGain{ 0.0f };
        float feedbackGain{ 0.0f };
        float plantStateGain{ 1.0f };
        float plantInputGain{ 0.0f };
        float disturbanceGain{ 0.0f };
        bool delayed{ false };
        bool primed{ false };
        RotatingFrame lastInput{ 0.0f, 0.0f };
        RotatingFrame expected{ 0.0f, 0.0f };
        RotatingFrame disturbance{ 0.0f, 0.0f };
    };

    static_assert(CurrentController<DeadbeatCurrentController>);
}
