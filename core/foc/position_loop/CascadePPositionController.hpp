#pragma once

#include "core/foc/interfaces/Algorithms.hpp"
#include "core/foc/position_loop/PositionController.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"

namespace foc
{
    class CascadePPositionController
    {
    public:
        static constexpr PositionAlgorithm algorithm{ PositionAlgorithm::cascadeP };

        bool Configure(const MechanicalModelParameters& motorParameters) const;
        bool SetTunings(const PositionLoopTunings& tunings);
        void Reset() const;

        OPTIMIZE_FOR_SPEED PositionOutput Compute(const PositionControlContext& context) const
        {
            return { PositionOutputKind::speedReference, PositionDifference(context.reference, context.measured, context.direct) * gain + context.referenceSpeed.Value() };
        }

    private:
        float gain{ PositionLoopTunings{}.bandwidth };
    };

    static_assert(PositionController<CascadePPositionController>);
}
