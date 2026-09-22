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

        bool Configure(const MechanicalModelParameters& motorParameters);
        bool SetTunings(const SpeedLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::Ampere Compute(const SpeedControlContext& context);

    private:
        using SpeedLqi = controllers::IntegralStateFeedbackLqi<float, 1, 1, 1>;

        static SpeedLqi Inert();
        bool Construct();

        MechanicalModelParameters parameters{};
        float bandwidth{ SpeedLoopTunings{}.bandwidth };
        float speedErrorWeight{ SpeedLoopTunings{}.speedErrorWeight };
        float integralWeight{ SpeedLoopTunings{}.integralWeight };
        SpeedLqi lqi{ Inert() };
    };

    static_assert(SpeedController<LqiSpeedController>);
}
