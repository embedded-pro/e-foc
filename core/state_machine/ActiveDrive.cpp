#include "core/state_machine/ActiveDrive.hpp"
#include "core/foc/math/WindingTemperature.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace state_machine
{
    ActiveDrive::ActiveDrive(const services::ConfigData& config)
        : config(config)
    {}

    bool ActiveDrive::HasMachine() const
    {
        return !std::holds_alternative<std::monostate>(machines);
    }

    ControlMode ActiveDrive::Active() const
    {
        if (std::holds_alternative<application::SpeedStateMachine>(machines))
            return ControlMode::speed;
        if (std::holds_alternative<application::PositionStateMachine>(machines))
            return ControlMode::position;
        really_assert(std::holds_alternative<application::TorqueStateMachine>(machines));
        return ControlMode::torque;
    }

    FocStateMachineBase& ActiveDrive::ActiveStateMachine()
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&machines))
            return *sm;
        if (auto* sm = std::get_if<application::PositionStateMachine>(&machines))
            return *sm;
        really_assert(std::holds_alternative<application::TorqueStateMachine>(machines));
        return std::get<application::TorqueStateMachine>(machines);
    }

    const FocStateMachineBase& ActiveDrive::ActiveStateMachine() const
    {
        if (const auto* sm = std::get_if<application::SpeedStateMachine>(&machines))
            return *sm;
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&machines))
            return *sm;
        really_assert(std::holds_alternative<application::TorqueStateMachine>(machines));
        return std::get<application::TorqueStateMachine>(machines);
    }

    application::FocStateMachineCommon& ActiveDrive::ActiveCommon()
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&machines))
            return *sm;
        if (auto* sm = std::get_if<application::PositionStateMachine>(&machines))
            return *sm;
        return std::get<application::TorqueStateMachine>(machines);
    }

    const application::FocStateMachineCommon& ActiveDrive::ActiveCommon() const
    {
        if (const auto* sm = std::get_if<application::SpeedStateMachine>(&machines))
            return *sm;
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&machines))
            return *sm;
        return std::get<application::TorqueStateMachine>(machines);
    }

    application::OuterLoopStateMachine* ActiveDrive::ActiveOuterLoop()
    {
        if (auto* sm = std::get_if<application::SpeedStateMachine>(&machines))
            return sm;
        if (auto* sm = std::get_if<application::PositionStateMachine>(&machines))
            return sm;
        return nullptr;
    }

    const application::OuterLoopStateMachine* ActiveDrive::ActiveOuterLoop() const
    {
        if (const auto* sm = std::get_if<application::SpeedStateMachine>(&machines))
            return sm;
        if (const auto* sm = std::get_if<application::PositionStateMachine>(&machines))
            return sm;
        return nullptr;
    }

    application::RotorReference ActiveDrive::RotorReferenceToKeep() const
    {
        if (!HasMachine())
            return application::RotorReference::lost;

        const auto* ready = std::get_if<Ready>(&ActiveStateMachine().CurrentState());
        if (ready == nullptr || !ready->rotorReferenceValid)
            return application::RotorReference::lost;

        return ready->homed ? application::RotorReference::keptHomed : application::RotorReference::kept;
    }

    std::optional<services::CalibrationData> ActiveDrive::ActiveCalibrationData() const
    {
        const auto& state = ActiveStateMachine().CurrentState();
        if (const auto* ready = std::get_if<state_machine::Ready>(&state))
            return ready->loadedData;
        return std::nullopt;
    }

    std::optional<foc::Celsius> ActiveDrive::WindingTemperature()
    {
        const auto estimates = ActiveStateMachine().CurrentOnlineEstimates();
        if (!estimates.has_value())
            return std::nullopt;

        return foc::WindingTemperatureFromResistance(estimates->resistance, foc::Ohm{ CalibrationRecord().rPhase }, foc::Celsius{ config.limits.calibrationTemperature });
    }

    bool ActiveDrive::IsHomed() const
    {
        return HasMachine() && ActiveCommon().IsHomed();
    }

    const services::CalibrationData& ActiveDrive::CalibrationRecord() const
    {
        return ActiveCommon().CalibrationRecord();
    }
}
