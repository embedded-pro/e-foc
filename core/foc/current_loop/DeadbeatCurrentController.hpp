#pragma once

#include "core/foc/current_loop/CurrentController.hpp"
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

        OPTIMIZE_FOR_SPEED foc::RotatingFrame Compute(const CurrentControlContext& context) const;

    private:
        void ApplyGains();

        MotorModelParameters parameters{};
        bool twoStep{ CurrentLoopTunings{}.twoStepDeadbeat };
        float referenceGain{ 0.0f };
        float feedbackGain{ 0.0f };
    };

    static_assert(CurrentController<DeadbeatCurrentController>);
}
