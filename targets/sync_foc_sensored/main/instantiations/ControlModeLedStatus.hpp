#pragma once

#include "core/services/leds/LedStatusSource.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include <optional>

namespace application
{
    class ControlModeLedStatus
        : public services::LedStatusSource
    {
    public:
        explicit ControlModeLedStatus(const std::optional<state_machine::ControlModeStateMachine>& controlMode);

        services::LedStatus Status() const override;

    private:
        const std::optional<state_machine::ControlModeStateMachine>& controlMode;
    };
}
