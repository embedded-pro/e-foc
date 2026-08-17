#pragma once

#include "core/foc/speed_loop/SpeedController.hpp"
#include "numerical/controllers/implementations/IntegralStateFeedbackLqi.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class LqiSpeedController
    {
    public:
        static constexpr SpeedAlgorithm algorithm{ SpeedAlgorithm::lqi };

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const SpeedLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::Ampere Compute(const SpeedControlContext& context);

    private:
        using SpeedLqi = controllers::IntegralStateFeedbackLqi<float, 1, 1, 1>;

        static SpeedLqi Inert();
        void Construct();

        MechanicalModelParameters parameters{};
        float speedErrorWeight{ SpeedLoopTunings{}.speedErrorWeight };
        float integralWeight{ SpeedLoopTunings{}.integralWeight };
        SpeedLqi lqi{ Inert() };
    };

    static_assert(SpeedController<LqiSpeedController>);
}
