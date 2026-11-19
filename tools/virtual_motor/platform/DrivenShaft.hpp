#pragma once

#include "e-foc-protocol/foc/Units.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace virtual_motor
{
    struct ShaftDrive
    {
        foc::RadiansPerSecond speed;
        uint64_t period;
        foc::NewtonMeter torque;
        foc::KilogramMeterSquared inertia;

        bool operator==(const ShaftDrive& other) const = default;
    };

    class DrivenShaft
    {
    public:
        static constexpr std::size_t historyPeriods{ 1024 };

        explicit DrivenShaft(float periodSeconds);

        void Drive(const std::optional<ShaftDrive>& drive);
        std::optional<foc::RadiansPerSecond> Speed() const;
        void Record(foc::NewtonMeter electricalTorque);

    private:
        float Backlog(const ShaftDrive& sent) const;

        float periodSeconds;
        std::array<float, historyPeriods> torques{};
        uint64_t period{ 0 };
        std::optional<ShaftDrive> drive;
        float impulseDeviation{ 0.0f };
    };
}
