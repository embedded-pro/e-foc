#pragma once

#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/position_loop/PidPositionController.hpp"

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
            shaped.reference = ShapedReference(context);

            return feedback.Compute(shaped);
        }

    private:
        void ApplyReferenceFilter();

        // An angle is not a linear quantity: the state advances along the wrapped error, so a setpoint
        // on the far side of the seam is approached the short way round instead of all the way back
        OPTIMIZE_FOR_SPEED Radians ShapedReference(const PositionControlContext& context)
        {
            if (!filterActive)
                return context.reference;

            if (!seeded)
            {
                shapedReference = context.measured.Value();
                seeded = true;
            }

            shapedReference = detail::PositionWithWrapAround(
                shapedReference + alpha * WrappedPositionError(context.reference, Radians{ shapedReference }));

            return Radians{ shapedReference };
        }

        PidPositionController feedback;
        hal::Hertz samplingFrequency{ 0 };
        float referenceTimeConstant{ PositionLoopTunings{}.referenceTimeConstant };
        float alpha{ 1.0f };
        float shapedReference{ 0.0f };
        bool filterActive{ false };
        bool seeded{ false };
    };

    static_assert(PositionController<TwoDofPositionController>);
}
