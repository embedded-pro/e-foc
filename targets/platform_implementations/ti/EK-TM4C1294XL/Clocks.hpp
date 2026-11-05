#pragma once

#include "hal_tiva/tiva/ClockTm4c129.hpp"
#include <cstdint>

namespace application::Clocks
{
    inline constexpr uint32_t systemClockFrequency{ 120000000 };

    inline void Initialize()
    {
        hal::tiva::ConfigureClock(systemClockFrequency, hal::tiva::crystalFrequency::_25_MHz, hal::tiva::oscillatorSource::main, hal::tiva::systemClockVco::_240_MHz, true);
    }
}
