#pragma once

#include "hal/interfaces/Pwm.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"

namespace application
{
    class PowerStageCutOff
    {
    public:
        static void Register(hal::ThreeChannelsPwm& pwm);
        static void Register(hal::SynchronousThreeChannelsPwm& pwm);
        static void Unregister();

        static void Cut();

    private:
        static hal::ThreeChannelsPwm* asynchronous;
        static hal::SynchronousThreeChannelsPwm* synchronous;
    };
}
