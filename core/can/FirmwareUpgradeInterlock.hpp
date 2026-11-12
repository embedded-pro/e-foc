#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeHold.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"

namespace can
{
    class FirmwareUpgradeInterlock
        : public services::FirmwareUpgradeHold
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
