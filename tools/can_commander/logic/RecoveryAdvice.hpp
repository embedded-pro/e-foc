#pragma once

#include "core/can/FocMotorMessages.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace tool
{
    enum class Refusal : uint8_t
    {
        invalidState,
        modeMismatch
    };

    enum class RecoveryAction : uint8_t
    {
        none,
        alignRotor,
        clearFault,
        stopMotor,
        showMotorParameters
    };

    struct RefusedCommand
    {
        uint8_t commandType;
        Refusal refusal;
        std::optional<can::FocMotorState> state;
        std::optional<can::FocMotorMode> mode;
        std::string_view faultName;
    };

    struct RecoveryAdvice
    {
        std::string text;
        RecoveryAction action{ RecoveryAction::none };

        bool operator==(const RecoveryAdvice&) const = default;
    };

    std::optional<RecoveryAdvice> AdviseRecovery(const RefusedCommand& refused);
    std::string_view RecoveryActionName(RecoveryAction action);
}
