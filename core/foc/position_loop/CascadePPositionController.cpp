#include "core/foc/position_loop/CascadePPositionController.hpp"

namespace foc
{
    bool CascadePPositionController::Configure(const MechanicalModelParameters&) const
    {
        return true;
    }

    bool CascadePPositionController::SetTunings(const PositionLoopTunings& tunings)
    {
        gain = tunings.bandwidth;
        return true;
    }

    void CascadePPositionController::Reset() const
    {
    }
}
