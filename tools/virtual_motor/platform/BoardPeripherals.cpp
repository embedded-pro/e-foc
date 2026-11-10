#include "tools/virtual_motor/platform/BoardPeripherals.hpp"
#include "infra/event/EventDispatcher.hpp"
#include <cstdio>

namespace virtual_motor
{
    bool StatePin::Get() const
    {
        return state;
    }

    void StatePin::Set(bool value)
    {
        state = value;
    }

    bool StatePin::GetOutputLatch() const
    {
        return state;
    }

    void StatePin::SetAsInput()
    {}

    bool StatePin::IsInput() const
    {
        return false;
    }

    void StatePin::Config(hal::PinConfigType)
    {}

    void StatePin::Config(hal::PinConfigType, bool startOutputState)
    {
        state = startOutputState;
    }

    void StatePin::ResetConfig()
    {}

    void StatePin::EnableInterrupt(const infra::Function<void()>&, hal::InterruptTrigger, hal::InterruptType)
    {}

    void StatePin::DisableInterrupt()
    {}

    void StandardOutputSerial::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {
        std::fwrite(data.begin(), 1, data.size(), stdout);
        std::fflush(stdout);
        infra::EventDispatcher::Instance().Schedule(actionOnCompletion);
    }

    void StandardOutputSerial::ReceiveData(infra::Function<void(infra::ConstByteRange data)>)
    {}

    void DeferredLowPriorityInterrupt::Trigger()
    {
        triggered = true;
    }

    void DeferredLowPriorityInterrupt::Register(const infra::Function<void()>& newHandler)
    {
        handler = newHandler;
    }

    void DeferredLowPriorityInterrupt::Unregister()
    {
        handler = nullptr;
        triggered = false;
    }

    void DeferredLowPriorityInterrupt::RunIfTriggered()
    {
        if (!triggered)
            return;

        triggered = false;
        if (handler)
            handler();
    }
}
