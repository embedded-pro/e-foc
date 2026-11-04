#include "tools/can_commander/logic/FirmwareRefusal.hpp"

namespace tool
{
    std::string FirmwareRefusalText(services::FwuError error, FirmwareStep step)
    {
        switch (error)
        {
            case services::FwuError::busy:
                return "the motor was not stopped or had work outstanding. Stop it, wait, and try again.";
            case services::FwuError::invalidSize:
                return "the pack does not fit its staging slot.";
            case services::FwuError::sequenceError:
                return "a block arrived out of order.";
            case services::FwuError::writeError:
                return "it could not write its staging slot.";
            case services::FwuError::crcMismatch:
                return "the pack did not arrive intact, and nothing was staged.";
            case services::FwuError::notReady:
                return step == FirmwareStep::apply ? "no pack is staged on it; download one first." : "the pack is not for this board and microcontroller, and nothing was staged.";
            case services::FwuError::invalidState:
                return "it was no longer receiving the pack: an emergency stop, an abort or 30 s without a block ended the download on the controller, and nothing was staged.";
            case services::FwuError::sessionTimeout:
                return "it ended the download after 30 s without a block, and nothing was staged.";
            default:
                return "it answered with code " + std::to_string(static_cast<int>(error)) + ".";
        }
    }
}
