#pragma once

#include <cstdint>

namespace application
{
    inline constexpr uint32_t kQemuSystemClockHz = 25000000u;

    inline constexpr uint32_t kDefaultBaseFrequencyHz = 20000u;
    inline constexpr float kDefaultSupplyVoltageVolts = 48.0f;

    inline constexpr uintptr_t kTerminalUartBase = 0x40004000u;
    inline constexpr uintptr_t kCanUartBase = 0x40005000u;
}
