#pragma once

#include "core/foc/position_loop/PidPositionController.hpp"
#include "numerical/filters/passive/ExponentialMovingAverage.hpp"

namespace foc
{
    // Shapes the setpoint before the feedback law, so tracking and disturbance rejection tune apart.
    class TwoDofPositionController
    {
    public:
        static constexpr PositionAlgorithm algorithm{ PositionAlgorithm::twoDof };

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const PositionLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED PositionOutput Compute(const PositionControlContext& context)
        {
            auto shaped = context;
            shaped.reference = Radians{ referenceFilter.Filter(context.reference.Value()) };

            return feedback.Compute(shaped);
        }

    private:
        void ApplyReferenceFilter();

        PidPositionController feedback;
        filters::passive::ExponentialMovingAverage<float> referenceFilter{ 1.0f };
        hal::Hertz samplingFrequency{ 0 };
        float referenceTimeConstant{ PositionLoopTunings{}.referenceTimeConstant };
    };

    static_assert(PositionController<TwoDofPositionController>);
}
