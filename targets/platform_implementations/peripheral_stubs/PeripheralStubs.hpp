#pragma once

#include "hal/interfaces/AdcMultiChannel.hpp"
#include "hal/interfaces/Can.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "hal/interfaces/SerialCommunication.hpp"
#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include <array>
#include <cstdint>

namespace application
{
    class SerialCommunicationStub
        : public hal::SerialCommunication
    {
    public:
        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;
    };

    class GpioPinStub
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
    };

    class AdcMultiChannelStub
        : public hal::AdcMultiChannel
    {
    public:
        void Measure(const infra::Function<void(Samples)>& onDone) override;
        void Stop() override;

    private:
        // 12-bit ADC mid-scale: the zero-current reading a bipolar current-sense amplifier reports.
        static constexpr uint16_t neutralCode = 2048;
        static constexpr std::array<uint16_t, 3> neutralSamples{ neutralCode, neutralCode, neutralCode };
    };

    class ThreeChannelsPwmStub
        : public hal::ThreeChannelsPwm
    {
    public:
        void SetBaseFrequency(hal::Hertz baseFrequency) override;
        void Stop() override;
        void Start(hal::DutyCycle dutyCycle1, hal::DutyCycle dutyCycle2, hal::DutyCycle dutyCycle3) override;
    };

    class SynchronousQuadratureEncoderStub
        : public hal::SynchronousQuadratureEncoder
    {
    public:
        uint32_t Position() override;
        uint32_t Resolution() override;
        MotionDirection Direction() override;
        uint32_t Speed() override;
    };

    class CanStub
        : public hal::Can
    {
    public:
        void SendData(Id id, const Message& data, const infra::Function<void(bool success)>& actionOnCompletion) override;
        void ReceiveData(const infra::Function<void(Id id, const Message& data)>& receivedAction) override;
    };
}
