#include "targets/platform_implementations/error_handling_cortex_m/PowerStageCutOff.hpp"

namespace application
{
    hal::ThreeChannelsPwm* PowerStageCutOff::asynchronous = nullptr;
    hal::SynchronousThreeChannelsPwm* PowerStageCutOff::synchronous = nullptr;

    void PowerStageCutOff::Register(hal::ThreeChannelsPwm& pwm)
    {
        asynchronous = &pwm;
    }

    void PowerStageCutOff::Register(hal::SynchronousThreeChannelsPwm& pwm)
    {
        synchronous = &pwm;
    }

    void PowerStageCutOff::Unregister()
    {
        asynchronous = nullptr;
        synchronous = nullptr;
    }

    void PowerStageCutOff::Cut()
    {
        if (asynchronous != nullptr)
            asynchronous->Stop();

        if (synchronous != nullptr)
            synchronous->Stop();
    }
}
