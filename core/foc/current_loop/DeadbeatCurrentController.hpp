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

        void Configure(const MotorModelParameters& motorParameters);
        void SetTunings(const CurrentLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED RotatingFrame Compute(const CurrentControlContext& context) const
        {
            const RotatingFrame inverted{ referenceGain * context.reference.d - feedbackGain * context.measured.d,
                referenceGain * context.reference.q - feedbackGain * context.measured.q };

            return LimitToModulationCircle(decoupling.Apply(inverted, context));
        }

    private:
        void ApplyGains();

        MotorModelParameters parameters{};
        DecouplingFeedforward decoupling;
        bool twoStep{ CurrentLoopTunings{}.twoStepDeadbeat };
        float referenceGain{ 0.0f };
        float feedbackGain{ 0.0f };
    };

    static_assert(CurrentController<DeadbeatCurrentController>);
}
