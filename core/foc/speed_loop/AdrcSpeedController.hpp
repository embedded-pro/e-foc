#pragma once

#include "core/foc/speed_loop/SpeedController.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "numerical/robust_control/ActiveDisturbanceRejection.hpp"

namespace foc
{
    class AdrcSpeedController
    {
    public:
        static constexpr SpeedAlgorithm algorithm{ SpeedAlgorithm::adrc };

        bool Configure(const MechanicalModelParameters& motorParameters);
        bool SetTunings(const SpeedLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::Ampere Compute(const SpeedControlContext& context);

    private:
        using SpeedAdrc = robust_control::ActiveDisturbanceRejectionControl<float, 1>;

        static SpeedAdrc Inert();
        bool Construct();

        MechanicalModelParameters parameters{};
        float bandwidth{ SpeedLoopTunings{}.bandwidth };
        float observerBandwidthRatio{ SpeedLoopTunings{}.observerBandwidthRatio };
        float lastApplied{ 0.0f };
        SpeedAdrc adrc{ Inert() };
    };

    static_assert(SpeedController<AdrcSpeedController>);
}
