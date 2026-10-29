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
            const auto predictedD = predictionStateGain * context.measured.d + predictionInputGain * lastInput.d;
            const auto predictedQ = predictionStateGain * context.measured.q + predictionInputGain * lastInput.q;
            const RotatingFrame command{ referenceGain * context.reference.d - feedbackGain * predictedD, referenceGain * context.reference.q - feedbackGain * predictedQ };

            const auto decoupled = decoupling.Apply(command, context);
            const auto applied = LimitToModulationCircle(decoupled);
            lastInput = RotatingFrame{ command.d + applied.d - decoupled.d, command.q + applied.q - decoupled.q };

            return applied;
        }

    private:
        bool ApplyGains();

        MotorModelParameters parameters{};
        DecouplingFeedforward decoupling;
        bool twoStep{ CurrentLoopTunings{}.twoStepDeadbeat };
        float referenceGain{ 0.0f };
        float feedbackGain{ 0.0f };
        float predictionStateGain{ 1.0f };
        float predictionInputGain{ 0.0f };
        RotatingFrame lastInput{ 0.0f, 0.0f };
    };

    static_assert(CurrentController<DeadbeatCurrentController>);
}
