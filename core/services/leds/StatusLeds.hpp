#pragma once

#include "core/services/leds/LedPatterns.hpp"
#include "core/services/leds/LedStatusSource.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/timer/Timer.hpp"
#include <chrono>

namespace services
{
    class StatusLeds
    {
    public:
        static constexpr std::chrono::milliseconds tickPeriod{ 50 };

        StatusLeds(hal::GpioPin& green, hal::GpioPin& amber, hal::GpioPin& red, const LedStatusSource& source);

    private:
        void Refresh();

        const LedStatusSource& source;
        LedPatternPlayer green;
        LedPatternPlayer amber;
        LedPatternPlayer red;
        infra::TimerRepeating timer;
    };
}
