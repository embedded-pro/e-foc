#pragma once

#include "core/services/speed_controllers/SpeedController.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "numerical/robust_control/ActiveDisturbanceRejection.hpp"

namespace services
{
    class AdrcSpeedController
    {
    public:
        static constexpr SpeedAlgorithm algorithm{ SpeedAlgorithm::adrc };

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const SpeedControllerTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::Ampere Compute(const SpeedControlContext& context);

    private:
        // The mechanical plant is first order; the extended state adds the lumped load torque
        using SpeedAdrc = robust_control::ActiveDisturbanceRejectionControl<float, 1>;

        static SpeedAdrc Inert();
        void Construct();

        MechanicalModelParameters parameters{};
        float bandwidth{ SpeedControllerTunings{}.bandwidth };
        float observerBandwidthRatio{ SpeedControllerTunings{}.observerBandwidthRatio };
        float lastApplied{ 0.0f };
        SpeedAdrc adrc{ Inert() };
    };

    static_assert(SpeedController<AdrcSpeedController>);
}
