#pragma once

#include "hal_st/stm32fxxx/GpioStm.hpp"
#include <cstdint>
#include DEVICE_HEADER

namespace application
{
    // Polled, oversampled conversions on a converter that no interrupt-driven driver owns; the pin's analog mode belongs to its other user
    class SynchronousAdcChannelStm
    {
    public:
        SynchronousAdcChannelStm(uint8_t oneBasedAdcIndex, hal::GpioPinStm& pin, uint32_t samplingTime);
        SynchronousAdcChannelStm(const SynchronousAdcChannelStm&) = delete;
        SynchronousAdcChannelStm& operator=(const SynchronousAdcChannelStm&) = delete;
        ~SynchronousAdcChannelStm();

        uint16_t Measure();

    private:
        uint8_t index;
        ADC_HandleTypeDef handle{};
    };
}
