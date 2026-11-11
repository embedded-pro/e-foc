#pragma once

#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/ActiveDrive.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include <optional>

namespace state_machine
{
    class DriveCommands
    {
    public:
        DriveCommands(ActiveDrive& drive, const services::ConfigData& config);

        bool TrySetTorque(foc::IdAndIqPoint setpoint);
        bool TrySetSpeed(foc::RadiansPerSecond setpoint);
        bool TrySetPosition(foc::Radians setpoint);
        void LimitCurrent(foc::Ampere limit);
        void TrackBusVoltage(foc::Volts measured);
        void ApplyLimits();

    private:
        void ApplyCurrentLimit();
        void ApplyBusVoltage();

        ActiveDrive& drive;
        const services::ConfigData& config;
        std::optional<foc::Ampere> currentLimit;
        std::optional<foc::Volts> busVoltage;
    };
}
