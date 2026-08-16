#pragma once

#include "core/services/speed_controllers/SpeedController.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace services
{
    class PidSpeedController
    {
    public:
        static constexpr SpeedAlgorithm algorithm{ SpeedAlgorithm::pid };

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const SpeedControllerTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::Ampere Compute(const SpeedControlContext& context);

    private:
        void ApplyGains();

        // The loop runs per-unit of the current envelope so the incremental clamp doubles as anti-windup
        controllers::PidIncrementalSynchronous<float> speedPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        MechanicalModelParameters parameters{};
        float bandwidth{ SpeedControllerTunings{}.bandwidth };
    };

    static_assert(SpeedController<PidSpeedController>);
}
