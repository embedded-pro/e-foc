#pragma once

#include "core/foc/speed_loop/SpeedController.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace foc
{
    class PidSpeedController
    {
    public:
        static constexpr SpeedAlgorithm algorithm{ SpeedAlgorithm::pid };

        bool Configure(const MechanicalModelParameters& motorParameters);
        bool SetTunings(const SpeedLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::Ampere Compute(const SpeedControlContext& context);

    private:
        bool ApplyGains();

        controllers::PidIncrementalSynchronous<float> speedPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        MechanicalModelParameters parameters{};
        float bandwidth{ SpeedLoopTunings{}.bandwidth };
    };

    static_assert(SpeedController<PidSpeedController>);
}
