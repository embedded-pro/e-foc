#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "integration_tests/software_in_the_loop/qemu_target/SilQemuApplication.hpp"
#include <cstdio>

namespace sil
{
    void NoOpSerialCommunication::SendData(infra::ConstByteRange, infra::Function<void()> onDone)
    {
        if (onDone)
            onDone();
    }

    void NoOpSerialCommunication::ReceiveData(infra::Function<void(infra::ConstByteRange)>)
    {}

    infra::TextOutputStream SinkTracer::Continue()
    {
        return infra::TextOutputStream::WithErrorPolicy{ dummy };
    }

    uint32_t ArrayEeprom::Size() const
    {
        return eepromSize;
    }

    void ArrayEeprom::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        for (std::size_t i = 0; i < buffer.size() && (address + i) < eepromSize; ++i)
            storage[address + i] = buffer[i];
        if (onDone)
            onDone();
    }

    void ArrayEeprom::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        for (std::size_t i = 0; i < buffer.size() && (address + i) < eepromSize; ++i)
            buffer[i] = storage[address + i];
        if (onDone)
            onDone();
    }

    void ArrayEeprom::Erase(infra::Function<void()> onDone)
    {
        storage.fill(0xFF);
        if (onDone)
            onDone();
    }

    void NoOpInverterEncoder::PhaseCurrentsReady(hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>&)
    {}

    void NoOpInverterEncoder::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles&)
    {}

    void NoOpInverterEncoder::Start()
    {}

    void NoOpInverterEncoder::Stop()
    {}

    hal::Hertz NoOpInverterEncoder::BaseFrequency() const
    {
        return hal::Hertz{ 20000 };
    }

    foc::Ampere NoOpInverterEncoder::MaxCurrentSupported() const
    {
        return foc::Ampere{ 15.0f };
    }

    foc::Radians NoOpInverterEncoder::Read()
    {
        return foc::Radians{ 0.0f };
    }

    void NoOpInverterEncoder::Set(foc::Radians)
    {}

    void NoOpInverterEncoder::SetZero()
    {}

    void NoOpLowPriorityInterrupt::Trigger()
    {}

    void NoOpLowPriorityInterrupt::Register(const infra::Function<void()>&)
    {}

    void NoOpLowPriorityInterrupt::Unregister()
    {}

    void NoOpPerformanceTracker::Start()
    {}

    uint32_t NoOpPerformanceTracker::ElapsedCycles()
    {
        return 0;
    }

    SilQemuApplication::SilQemuApplication()
    {
        canServer.RegisterCategory(motorCategoryServer);
    }

    void SilQemuApplication::Run()
    {
        std::puts("READY");
        std::fflush(stdout);

        while (true)
        {
            eventDispatcher.ExecuteAllActions();
            can.PollIncoming();
        }
    }
}
