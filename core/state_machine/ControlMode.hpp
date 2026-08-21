#pragma once

#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
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

    inline can::FocMotorMode ToCanMode(ControlMode mode)
    {
        switch (mode)
        {
            case ControlMode::speed:
                return can::FocMotorMode::speed;
            case ControlMode::position:
                return can::FocMotorMode::position;
            case ControlMode::torque:
            default:
                return can::FocMotorMode::torque;
        }
    }

    inline std::optional<ControlMode> FromCanMode(can::FocMotorMode mode)
    {
        switch (mode)
        {
            case can::FocMotorMode::torque:
                return ControlMode::torque;
            case can::FocMotorMode::speed:
                return ControlMode::speed;
            case can::FocMotorMode::position:
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

    inline can::FocMotorCategoryError ToCategoryError(SelectResult result)
    {
        switch (result)
        {
            case SelectResult::nvmFailed:
                return can::FocMotorCategoryError::persistenceFailed;
            case SelectResult::busy:
            default:
                return can::FocMotorCategoryError::busy;
        }
    }

    inline can::FocMotorCategoryError ToCategoryError(CommandResult result)
    {
        switch (result)
        {
            case CommandResult::calibrationFailed:
                return can::FocMotorCategoryError::calibrationFailed;
            case CommandResult::nvmFailed:
                return can::FocMotorCategoryError::persistenceFailed;
            case CommandResult::abortedByFault:
                return can::FocMotorCategoryError::abortedByFault;
            case CommandResult::rejected:
            default:
                return can::FocMotorCategoryError::modeMismatch;
        }
    }
}
