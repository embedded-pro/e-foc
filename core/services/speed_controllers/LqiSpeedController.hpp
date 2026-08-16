#pragma once

#include "core/services/speed_controllers/SpeedController.hpp"
#include "numerical/controllers/implementations/IntegralStateFeedbackLqi.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace services
{
    class LqiSpeedController
    {
    public:
        static constexpr SpeedAlgorithm algorithm{ SpeedAlgorithm::lqi };

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const SpeedControllerTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::Ampere Compute(const SpeedControlContext& context);

    private:
        using SpeedLqi = controllers::IntegralStateFeedbackLqi<float, 1, 1, 1>;

        static SpeedLqi Inert();
        void Construct();

        MechanicalModelParameters parameters{};
        float speedErrorWeight{ SpeedControllerTunings{}.speedErrorWeight };
        float integralWeight{ SpeedControllerTunings{}.integralWeight };
        bool integratorHalted{ false };
        SpeedLqi lqi{ Inert() };
    };

    static_assert(SpeedController<LqiSpeedController>);
}
