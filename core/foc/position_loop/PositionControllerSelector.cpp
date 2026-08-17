#include "core/foc/position_loop/PositionControllerSelector.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    // The state feedback laws are only selectable once their Riccati design actually converges,
    // so a rejected solve leaves the previously active algorithm running instead of a dead loop.
    bool PositionControllerTraits::IsSelectable(PositionAlgorithm algorithm, const MechanicalModelParameters& parameters, const PositionLoopTunings& tunings)
    {
        switch (algorithm)
        {
            case PositionAlgorithm::lqr:
                return LqrPositionController::IsDesignFeasible(parameters, tunings);
            case PositionAlgorithm::lqi:
                return LqiPositionController::IsDesignFeasible(parameters, tunings);
            default:
                return true;
        }
    }

    SelectResult PositionControllerSelector::Select(PositionAlgorithm algorithm)
    {
        using enum PositionAlgorithm;

        switch (algorithm)
        {
            case pid:
                return Select<PidPositionController>();
            case cascadeP:
                return Select<CascadePPositionController>();
            case lqr:
                return Select<LqrPositionController>();
            case lqi:
                return Select<LqiPositionController>();
            case twoDof:
                return Select<TwoDofPositionController>();
            default:
                return SelectResult::invalidAlgorithm;
        }
    }
}
