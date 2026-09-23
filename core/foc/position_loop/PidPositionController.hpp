#pragma once

#include "core/foc/interfaces/Algorithms.hpp"
#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/foc/position_loop/PositionController.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <numbers>

namespace foc
{
    class PidPositionController
    {
    public:
        static constexpr PositionAlgorithm algorithm{ PositionAlgorithm::pid };
        static constexpr float integralZeroRatio{ 0.2f };
        static constexpr float referenceWeight{ 0.75f };

        bool Configure(const MechanicalModelParameters& motorParameters);
        bool SetTunings(const PositionLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED PositionOutput Compute(const PositionControlContext& context);

    private:
        float SpeedEnvelope() const;
        bool ApplyGains();

        static constexpr float maximumErrorInRadians{ std::numbers::pi_v<float> };

        float proportionalGain{ 0.0f };
        float integralGain{ 0.0f };
        float output{ 0.0f };
        float previousReference{ 0.0f };
        float previousMeasured{ 0.0f };
        bool primed{ false };
        PositionLoopTunings tunings{};
        hal::Hertz samplingFrequency{ 0 };
    };

    static_assert(PositionController<PidPositionController>);
}
