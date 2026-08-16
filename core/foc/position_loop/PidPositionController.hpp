#pragma once

#include "core/foc/position_loop/PositionPlantModel.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"

namespace foc
{
    // Proportional-integral position law whose output is a speed reference, so the cascade keeps
    // the existing speed and current loops underneath and the tuning stays independent of the load.
    class PidPositionController
    {
    public:
        static constexpr PositionAlgorithm algorithm{ PositionAlgorithm::pid };

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const PositionLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED PositionOutput Compute(const PositionControlContext& context);

    private:
        float SpeedEnvelope() const;
        void ApplyGains();

        // The proportional law saturates one bandwidth of speed reference at half a turn of error
        static constexpr float maximumErrorInRadians{ std::numbers::pi_v<float> };

        // The loop runs per-unit of the speed envelope so the incremental clamp doubles as anti-windup
        controllers::PidIncrementalSynchronous<float> positionPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        PositionLoopTunings tunings{};
        hal::Hertz samplingFrequency{ 0 };
    };

    static_assert(PositionController<PidPositionController>);
}
