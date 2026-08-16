#include "core/foc/position_loop/CascadePPositionController.hpp"

namespace foc
{
    void CascadePPositionController::Configure(const MechanicalModelParameters&)
    {
    }

    void CascadePPositionController::SetTunings(const PositionLoopTunings& tunings)
    {
        gain = tunings.bandwidth;
    }

    void CascadePPositionController::Reset()
    {
    }
}
