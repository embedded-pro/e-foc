#pragma once

#include "core/services/firmware_upgrade/FirmwareUpgradeDrive.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"

namespace can
{
    class FirmwareUpgradeInterlock
        : public services::FirmwareUpgradeDrive
    {
    public:
        explicit FirmwareUpgradeInterlock(state_machine::ControlModeStateMachine& controlMode);

        bool IsStoppedAndIdle() const override;
        void Hold() override;
        void Release() override;

    private:
        state_machine::ControlModeStateMachine& controlMode;
    };
}
