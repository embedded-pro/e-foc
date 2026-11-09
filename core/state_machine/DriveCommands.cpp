#include "core/state_machine/DriveCommands.hpp"
#include <algorithm>
#include <type_traits>

namespace state_machine
{
    DriveCommands::DriveCommands(ActiveDrive& drive, const services::ConfigData& config)
        : drive(drive)
        , config(config)
    {}

    bool DriveCommands::TrySetTorque(foc::IdAndIqPoint setpoint)
    {
        bool set{ false };
        drive.Visit([&]<class Machine>(Machine& sm)
            {
                if constexpr (std::is_same_v<Machine, application::TorqueStateMachine>)
                {
                    sm.GetController().SetPoint(setpoint);
                    set = true;
                }
            });
        return set;
    }

    bool DriveCommands::TrySetSpeed(foc::RadiansPerSecond setpoint)
    {
        const auto limit = config.limits.overspeedLimit;
        bool set{ false };
        drive.Visit([&]<class Machine>(Machine& sm)
            {
                if constexpr (std::is_same_v<Machine, application::SpeedStateMachine>)
                {
                    sm.GetController().SetPoint(foc::RadiansPerSecond{ std::clamp(setpoint.Value(), -limit, limit) });
                    set = true;
                }
            });
        return set;
    }

    bool DriveCommands::TrySetPosition(foc::Radians setpoint)
    {
        bool set{ false };
        drive.Visit([&]<class Machine>(Machine& sm)
            {
                if constexpr (std::is_same_v<Machine, application::PositionStateMachine>)
                {
                    sm.GetController().SetPoint(setpoint);
                    set = true;
                }
            });
        return set;
    }

    void DriveCommands::LimitCurrent(foc::Ampere limit)
    {
        currentLimit = limit;
        ApplyCurrentLimit();
    }

    void DriveCommands::TrackBusVoltage(foc::Volts measured)
    {
        busVoltage = measured;
        ApplyBusVoltage();
    }

    void DriveCommands::ApplyLimits()
    {
        const foc::RadiansPerSecond overspeedLimit{ config.limits.overspeedLimit };
        const foc::RadiansPerSecondSquared acceleration{ config.limits.speedAcceleration };
        const foc::RadiansPerSecondSquared deceleration{ config.limits.speedDeceleration };
        const foc::Radians positionMinimum{ config.limits.positionMinimum };
        const foc::Radians positionMaximum{ config.limits.positionMaximum };
        const bool homingRequired = config.homingRequired != 0;

        drive.Visit([&]<class Machine>(Machine& sm)
            {
                if constexpr (std::is_same_v<Machine, application::TorqueStateMachine>)
                    sm.SpeedLimiter().LimitSpeed(overspeedLimit);
                else if constexpr (!std::is_same_v<Machine, std::monostate>)
                {
                    sm.RampSpeed(acceleration, deceleration);
                    sm.LimitTravel(positionMinimum, positionMaximum);
                    if constexpr (std::is_same_v<Machine, application::PositionStateMachine>)
                        sm.RequireHoming(homingRequired);
                }
            });

        ApplyCurrentLimit();
        ApplyBusVoltage();
    }

    void DriveCommands::ApplyBusVoltage()
    {
        if (!busVoltage.has_value())
            return;

        drive.Visit([measured = *busVoltage]<class Machine>(Machine& sm)
            {
                if constexpr (std::is_same_v<Machine, application::TorqueStateMachine>)
                    sm.BusVoltageTracker().TrackBusVoltage(measured);
                else if constexpr (!std::is_same_v<Machine, std::monostate>)
                    sm.TrackBusVoltage(measured);
            });
    }

    void DriveCommands::ApplyCurrentLimit()
    {
        if (!currentLimit.has_value())
            return;

        drive.Visit([limit = *currentLimit]<class Machine>(Machine& sm)
            {
                if constexpr (std::is_same_v<Machine, application::TorqueStateMachine>)
                    sm.CurrentLimiter().LimitCurrent(limit);
                else if constexpr (!std::is_same_v<Machine, std::monostate>)
                    sm.LimitCurrent(limit);
            });
    }
}
