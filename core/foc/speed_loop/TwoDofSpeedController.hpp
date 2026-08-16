#pragma once

#include "core/foc/speed_loop/PidSpeedController.hpp"
#include "numerical/filters/passive/ExponentialMovingAverage.hpp"

namespace foc
{
    class TwoDofSpeedController
    {
    public:
        static constexpr SpeedAlgorithm algorithm{ SpeedAlgorithm::twoDof };

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const SpeedLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::Ampere Compute(const SpeedControlContext& context);

    private:
        void ApplyReferenceFilter();

        PidSpeedController feedback;
        filters::passive::ExponentialMovingAverage<float> referenceFilter{ 1.0f };
        hal::Hertz samplingFrequency{ 0 };
        float referenceTimeConstant{ SpeedLoopTunings{}.referenceTimeConstant };
    };

    static_assert(SpeedController<TwoDofSpeedController>);
}
