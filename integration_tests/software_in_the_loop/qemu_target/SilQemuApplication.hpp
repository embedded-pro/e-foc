#pragma once

#include "can-lite/server/CanProtocolServer.hpp"
#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "hal/interfaces/Can.hpp"
#include "hal/interfaces/Eeprom.hpp"
#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/util/Function.hpp"
#include "integration_tests/software_in_the_loop/qemu_target/SemihostingCan.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <array>
#include <cstdint>

namespace sil
{
    class NoOpSerialCommunication
        : public hal::SerialCommunication
    {
    public:
        void SendData(infra::ConstByteRange, infra::Function<void()> onDone) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange)>) override;
    };

    class SinkTracer
        : public services::Tracer
    {
    public:
        infra::TextOutputStream Continue() override;

    private:
        infra::StreamWriterDummy dummy;
        infra::TextOutputStream::WithErrorPolicy dummyStream{ dummy };
    };

    class ArrayEeprom
        : public hal::Eeprom
    {
    public:
        static constexpr uint32_t eepromSize = 256;

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

    private:
        std::array<uint8_t, eepromSize> storage{};
    };

    class NoOpInverterEncoder
        : public drivers::ThreePhaseInverter
        , public drivers::Encoder
    {
    public:
        void PhaseCurrentsReady(hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>&) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles&) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        foc::Radians Read() override;
        void Set(foc::Radians) override;
        void SetZero() override;
    };

    class NoOpLowPriorityInterrupt
        : public foc::LowPriorityInterrupt
    {
    public:
        void Trigger() override;
        void Register(const infra::Function<void()>&) override;
        void Unregister() override;
    };

    class NoOpPerformanceTracker
        : public hal::PerformanceTracker
    {
    public:
        void Start() override;
        uint32_t ElapsedCycles() override;
    };

    struct SilQemuApplication
    {
        SilQemuApplication();

        void Run();

        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        SemihostingCan can;

        NoOpSerialCommunication serialComm;
        SinkTracer tracer;
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<32, 2> terminalWithCommands{ serialComm, tracer };
        services::TerminalWithStorage::WithMaxSize<20> terminalWithStorage{ terminalWithCommands, tracer };

        ArrayEeprom eeprom;
        services::NvmEepromRegion calibrationRegion{ eeprom, 0, 128 };
        services::NvmEepromRegion configRegion{ eeprom, 128, 128 };
        services::NonVolatileMemoryImpl nvm{ calibrationRegion, configRegion };

        NoOpInverterEncoder inverterEncoder;
        NoOpLowPriorityInterrupt lowPriInterrupt;

        services::ElectricalParametersIdentificationImpl electricalIdent{
            inverterEncoder, inverterEncoder, foc::Volts{ 48.0f }
        };
        services::MotorAlignmentImpl motorAlignment{ inverterEncoder, inverterEncoder };

        state_machine::NoOpFaultNotifier faultNotifier;

        services::ConfigData configData{};

        state_machine::ControlModeStateMachine controlMode{
            application::TerminalAndTracer{ terminalWithStorage, tracer },
            application::MotorHardware{ inverterEncoder, inverterEncoder, foc::Volts{ 48.0f } },
            nvm,
            application::CalibrationServices{ electricalIdent, motorAlignment },
            faultNotifier,
            configData,
            application::OuterLoopArgs{
                foc::Ampere{ 15.0f },
                hal::Hertz{ 20000 },
                lowPriInterrupt
            }
        };

        services::CanProtocolServer canServer{ can, services::CanProtocolServer::Config{ .nodeId = 1 } };
        can::FocMotorCategoryServer motorCategoryServer{ canServer.Transport() };

        can::FocMotorCanBridge canBridge{
            motorCategoryServer,
            controlMode,
            inverterEncoder,
            electricalIdent,
            nullptr,
            foc::NewtonMeter{ 0.1f },
            nvm,
            configData,
            tracer
        };
    };
}
