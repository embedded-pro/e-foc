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
            // The sliding surface is designed on the decoupled RL plant, which leaves the
            // cross-coupling and the back-EMF as disturbance for the switching term to reject. At
            // 5000 rpm the back-EMF alone is around 21 V against the 2.8 V the default switching
            // gain commands, so the surface is never reached and the axis tracks nothing. Feeding
            // them forward - the same component the decoupled PID uses - leaves the switching term
            // the residue it is sized for, and is why context.electricalSpeed is carried at all.
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
