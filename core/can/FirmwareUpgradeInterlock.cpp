#include "core/can/FirmwareUpgradeInterlock.hpp"

namespace can
{
    FirmwareUpgradeInterlock::FirmwareUpgradeInterlock(state_machine::ControlModeStateMachine& controlMode)
        : controlMode(controlMode)
    {}

    bool FirmwareUpgradeInterlock::IsStoppedAndIdle() const
    {
        return controlMode.IsStoppedAndIdle();
    }

    void FirmwareUpgradeInterlock::Hold()
    {
        controlMode.HoldAsyncWork();
    }

    void FirmwareUpgradeInterlock::Release()
    {
        controlMode.ReleaseAsyncWork();
    }
}
