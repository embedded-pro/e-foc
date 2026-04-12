#pragma once

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include <gmock/gmock.h>

namespace hal
{
    class SynchronousThreeChannelsPwmMock
        : public SynchronousThreeChannelsPwm
    {
    public:
        MOCK_METHOD(void, SetBaseFrequency, (Hertz baseFrequency), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(void, Start, (Percent dutyCycle1, Percent dutyCycle2, Percent dutyCycle3), (override));
    };
}
