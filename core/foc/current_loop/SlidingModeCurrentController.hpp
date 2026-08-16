#pragma once

#include "core/foc/current_loop/CurrentController.hpp"
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
        void Reset();

        OPTIMIZE_FOR_SPEED foc::RotatingFrame Compute(const CurrentControlContext& context);

    private:
        using ScalarSlidingMode = robust_control::SlidingModeControl<float, 1, 1>;

        static ScalarSlidingMode Inert();
        void Construct();
        OPTIMIZE_FOR_SPEED float ComputeAxis(float measured, float reference);

        MotorModelParameters parameters{};
        float switchingGain{ CurrentLoopTunings{}.switchingGain };
        float boundaryLayer{ CurrentLoopTunings{}.boundaryLayer };
        float normalizationScale{ 0.0f };
        ScalarSlidingMode slidingMode{ Inert() };
    };

    static_assert(CurrentController<SlidingModeCurrentController>);
}
