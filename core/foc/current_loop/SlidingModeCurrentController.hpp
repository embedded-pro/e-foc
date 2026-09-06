#pragma once

#include "core/foc/current_loop/CurrentController.hpp"
#include "core/foc/current_loop/CurrentPlantModel.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "numerical/robust_control/SlidingModeControl.hpp"

namespace foc
{
    class SlidingModeCurrentController
    {
    public:
        static constexpr CurrentAlgorithm algorithm{ CurrentAlgorithm::slidingMode };

        void Configure(const MotorModelParameters& motorParameters);
        void SetTunings(const CurrentLoopTunings& tunings);
        void Reset() const;

        OPTIMIZE_FOR_SPEED RotatingFrame Compute(const CurrentControlContext& context)
        {
            const RotatingFrame proposed{ ComputeAxis(context.measured.d, context.reference.d),
                ComputeAxis(context.measured.q, context.reference.q) };

            return LimitToModulationCircle(decoupling.Apply(proposed, context));
        }

    private:
        using ScalarSlidingMode = robust_control::SlidingModeControl<float, 1, 1>;

        static ScalarSlidingMode Inert();
        void Construct();

        OPTIMIZE_FOR_SPEED float ComputeAxis(float measured, float reference)
        {
            const auto control = slidingMode.ComputeControl(ScalarSlidingMode::StateVector{ measured },
                ScalarSlidingMode::StateVector{ reference });

            return (control.at(0, 0) + equilibriumGain * reference) * normalizationScale;
        }

        MotorModelParameters parameters{};
        float switchingGain{ CurrentLoopTunings{}.switchingGain };
        float boundaryLayer{ CurrentLoopTunings{}.boundaryLayer };
        float normalizationScale{ 0.0f };
        float equilibriumGain{ 0.0f };
        DecouplingFeedforward decoupling;
        ScalarSlidingMode slidingMode{ Inert() };
    };

    static_assert(CurrentController<SlidingModeCurrentController>);
}
