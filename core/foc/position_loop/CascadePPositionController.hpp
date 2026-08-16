#pragma once

#include "core/foc/position_loop/PositionPlantModel.hpp"

namespace foc
{
    class CascadePPositionController
    {
    public:
        static constexpr PositionAlgorithm algorithm{ PositionAlgorithm::cascadeP };

        void Configure(const MechanicalModelParameters& motorParameters);
        void SetTunings(const PositionLoopTunings& tunings);
        void Reset();

        OPTIMIZE_FOR_SPEED PositionOutput Compute(const PositionControlContext& context)
        {
            return { PositionOutputKind::speedReference, WrappedPositionError(context.reference, context.measured) * gain };
        }

    private:
        float gain{ PositionLoopTunings{}.bandwidth };
    };

    static_assert(PositionController<CascadePPositionController>);
}
