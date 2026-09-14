#pragma once

#include "core/foc/interfaces/Algorithms.hpp"
#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/foc/position_loop/PositionController.hpp"
#include "numerical/controllers/implementations/PidIncremental.hpp"

namespace foc
{
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

        static constexpr float maximumErrorInRadians{ std::numbers::pi_v<float> };

        controllers::PidIncrementalSynchronous<float> positionPid{ { 0.0f, 0.0f, 0.0f }, { -1.0f, 1.0f } };
        PositionLoopTunings tunings{};
        hal::Hertz samplingFrequency{ 0 };
    };

    static_assert(PositionController<PidPositionController>);
}
