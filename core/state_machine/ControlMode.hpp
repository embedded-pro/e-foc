#pragma once

#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include <cstdint>
#include <optional>

namespace state_machine
{
    enum class ControlMode : uint8_t
    {
        torque = 0,
        speed = 1,
        position = 2
    };

    enum class SelectResult : uint8_t
    {
        ok = 0,
        busy = 1,
        invalidMode = 2,
        nvmFailed = 3
    };

    inline ControlMode ControlModeFromRaw(uint8_t raw)
    {
        switch (raw)
        {
            case static_cast<uint8_t>(ControlMode::speed):
                return ControlMode::speed;
            case static_cast<uint8_t>(ControlMode::position):
                return ControlMode::position;
            case static_cast<uint8_t>(ControlMode::torque):
            default:
                return ControlMode::torque;
        }
    }

    inline services::FocMotorMode ToCanMode(ControlMode mode)
    {
        switch (mode)
        {
            case ControlMode::speed:
                return services::FocMotorMode::speed;
            case ControlMode::position:
                return services::FocMotorMode::position;
            case ControlMode::torque:
            default:
                return services::FocMotorMode::torque;
        }
    }

    inline std::optional<ControlMode> FromCanMode(services::FocMotorMode mode)
    {
        switch (mode)
        {
            case services::FocMotorMode::torque:
                return ControlMode::torque;
            case services::FocMotorMode::speed:
                return ControlMode::speed;
            case services::FocMotorMode::position:
                return ControlMode::position;
        }

        return std::nullopt;
    }

    inline services::CanAckStatus ToAckStatus(SelectResult result)
    {
        switch (result)
        {
            case SelectResult::ok:
                return services::CanAckStatus::success;
            case SelectResult::invalidMode:
                return services::CanAckStatus::invalidPayload;
            case SelectResult::busy:
            case SelectResult::nvmFailed:
            default:
                return services::CanAckStatus::categoryError;
        }
    }

    inline services::FocMotorCategoryError ToCategoryError(SelectResult result)
    {
        switch (result)
        {
            case SelectResult::nvmFailed:
                return services::FocMotorCategoryError::persistenceFailed;
            case SelectResult::busy:
            default:
                return services::FocMotorCategoryError::busy;
        }
    }

    inline services::FocMotorCategoryError ToCategoryError(CommandResult result)
    {
        switch (result)
        {
            case CommandResult::calibrationFailed:
                return services::FocMotorCategoryError::calibrationFailed;
            case CommandResult::nvmFailed:
                return services::FocMotorCategoryError::persistenceFailed;
            case CommandResult::abortedByFault:
                return services::FocMotorCategoryError::abortedByFault;
            case CommandResult::rejected:
            default:
                return services::FocMotorCategoryError::modeMismatch;
        }
    }
}
