#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/position_loop/PositionControllerSelector.hpp"
#include "core/foc/interfaces/LoopTunings.hpp"

namespace foc
{
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
