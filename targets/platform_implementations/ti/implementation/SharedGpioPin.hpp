#pragma once

#include "hal_tiva/tiva/Gpio.hpp"

namespace application
{
    // A second handle on a pin another GpioPin owns: an AnalogPin built on it converts the same channel
    // without reserving the pin again or resetting it when it goes away.
    class SharedGpioPin
        : public hal::tiva::GpioPin
    {
    public:
        using hal::tiva::GpioPin::GpioPin;

        void ConfigAnalog() override
        {}

        void ResetConfig() override
        {}
    };
}
