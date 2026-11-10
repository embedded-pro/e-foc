#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/Function.hpp"

namespace virtual_motor
{
    class StatePin
        : public hal::GpioPin
    {
    public:
        bool Get() const override;
        void Set(bool value) override;
        bool GetOutputLatch() const override;
        void SetAsInput() override;
        bool IsInput() const override;
        void Config(hal::PinConfigType config) override;
        void Config(hal::PinConfigType config, bool startOutputState) override;
        void ResetConfig() override;
        void EnableInterrupt(const infra::Function<void()>& action, hal::InterruptTrigger trigger, hal::InterruptType type) override;
        void DisableInterrupt() override;

    private:
        bool state{ false };
    };

    class StandardOutputSerial
        : public hal::SerialCommunication
    {
    public:
        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;
    };

    class DeferredLowPriorityInterrupt
        : public foc::LowPriorityInterrupt
    {
    public:
        void Trigger() override;
        void Register(const infra::Function<void()>& handler) override;
        void Unregister() override;

        void RunIfTriggered();

    private:
        infra::Function<void()> handler;
        bool triggered{ false };
    };
}
