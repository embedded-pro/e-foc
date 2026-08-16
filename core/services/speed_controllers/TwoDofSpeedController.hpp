#pragma once

#include "core/services/speed_controllers/PidSpeedController.hpp"
#include "numerical/filters/passive/ExponentialMovingAverage.hpp"

namespace services
{
    class TwoDofSpeedController
    {
    public:
        static constexpr SpeedAlgorithm algorithm{ SpeedAlgorithm::twoDof };

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const SpeedControllerTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED foc::Ampere Compute(const SpeedControlContext& context);

    private:
        void ApplyReferenceFilter();

        PidSpeedController feedback;
        filters::passive::ExponentialMovingAverage<float> referenceFilter{ 1.0f };
        hal::Hertz samplingFrequency{ 0 };
        float referenceTimeConstant{ SpeedControllerTunings{}.referenceTimeConstant };
    };

    static_assert(SpeedController<TwoDofSpeedController>);
}
