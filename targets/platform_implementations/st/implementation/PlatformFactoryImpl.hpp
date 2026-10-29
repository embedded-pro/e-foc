#pragma once

#include <array>
#include <optional>
#include HARDWARE_PINS_AND_PERIPHERALS_HEADER
#include MOTOR_BOARD_CHARACTERISTICS_HEADER
#include "core/platform_abstraction/AdcPhaseCurrentMeasurement.hpp"
#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/LowSideShuntWindow.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/platform_abstraction/QuadratureEncoderDecorator.hpp"
#include "core/services/cli/SerialCommunicationMirror.hpp"
#include "drivers/eeprom/Eeprom24xx.hpp"
#include "hal/cortex_m/DataWatchpointAndTrace.hpp"
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "hal_st/stm32fxxx/AnalogToDigitalPinStm.hpp"
#include "hal_st/stm32fxxx/DmaStm.hpp"
#include "hal_st/stm32fxxx/FdCanStm.hpp"
#include "hal_st/stm32fxxx/GpioStm.hpp"
#include "hal_st/stm32fxxx/I2cStm.hpp"
#include "hal_st/stm32fxxx/PwmStm.hpp"
#include "hal_st/stm32fxxx/UartStmDma.hpp"
#include "hal_st/stm32fxxx/WatchDogStm.hpp"
#include "hal_st/synchronous_stm32fxxx/SynchronousAdcStm.hpp"
#include "hal_st/synchronous_stm32fxxx/SynchronousQuadratureEncoderStm.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "services/tracer/SerialCommunicationOnSeggerRtt.hpp"
#include "services/tracer/StreamWriterOnSerialCommunication.hpp"
#include "services/tracer/TracerWithDateTime.hpp"
#include "services/util/EventDispatcherWatchdog.hpp"
#include "targets/platform_implementations/cortex_m_common/CycleCounter.hpp"
#include "targets/platform_implementations/cortex_m_common/FocLowPriorityInterruptAdapter.hpp"
#include "targets/platform_implementations/error_handling_cortex_m/PersistentFaultData.hpp"
#include "targets/platform_implementations/st/implementation/AdcTriggeredByPwmStm.hpp"

extern "C" uint32_t SystemCoreClock;

namespace application
{
    struct InterruptPriorities
    {
        static constexpr auto phaseCurrentAdc{ hal::cortex::InterruptPriority::highest };
        static constexpr auto can{ hal::cortex::InterruptPriority::normal };
        static constexpr auto uart{ hal::cortex::InterruptPriority::low };
    };

    class PlatformFactoryImpl
        : public PlatformFactory
    {
    public:
        explicit PlatformFactoryImpl(const infra::Function<void()>& onInitialized);

        void ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold) override;
        drivers::DeadTimeLoss InverterDeadTimeLoss(std::chrono::nanoseconds deadTime) const override;
        uint32_t DutyUpdateDelayPeriods() const override;
        void SetEncoderResolution(uint32_t resolution) override;
        void ConfigureCanBus(uint32_t bitRate, bool testMode) override;
        CanBusAdapter& CanBus() override;

        void Run() override;
        services::Tracer& Tracer() override;
        services::TerminalWithCommands& Terminal() override;
        hal::GpioPin& OperationalLed() override;
        hal::GpioPin& WarningLed() override;
        hal::GpioPin& FailureLed() override;
        uint8_t BoardId() const override;
        bool PowerStatus() const override;
        hal::PerformanceTracker& PerformanceTimer() override;
        hal::Hertz SystemClock() const override;
        foc::Volts PowerSupplyVoltage() override;
        foc::LowPriorityInterrupt& LowPriorityInterrupt() override;
        hal::Eeprom& Eeprom() override;
        void RegisterBoardProtection(const infra::Function<void(PlatformFactory::BoardProtectionReason)>& onProtection) override;
        PlatformFactory::BoardProtectionState BoardProtectionStatus() override;
        void Reset() override;
        ResetCause GetResetCause() const override;
        infra::BoundedConstString FaultStatus() const override;
        PlatformDiagnostics& Diagnostics() override;

        void PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        static_assert(Resources::faultStatusSize >= PersistentFaultData::kMaxFormattedSize,
            "fault status must hold the full register, FSR/FAR and backtrace dump");

        // Constructed first: everything after it registers interrupts, configures pins or reads SystemCoreClock
        struct HalInitialization
        {
            HalInitialization();
        };

        struct Cortex
        {
            static constexpr infra::Duration watchdogExpirationTimeout{ std::chrono::milliseconds(100) };

            static hal::WatchDogStm::Config WatchdogConfig();

            explicit Cortex(PlatformFactoryImpl& platform);

            hal::WatchDogStm watchdog{ WatchdogConfig() };
            services::EventDispatcherWithWeakPtrAndWatchdog::WithSize<Resources::eventDispatcherSize> eventDispatcher;
            hal::cortex::DataWatchpointAndTrace dataWatchPointAndTrace;
            hal::cortex::SystemTickTimerService systemTick{ SystemCoreClock, std::chrono::milliseconds(1) };
        };

        struct TerminalAndTracer
        {
            hal::DmaStm::TransmitStream transmitStream;
            hal::UartStmDma uart;
#ifdef E_FOC_CONSOLE_ON_RTT
            services::SerialCommunicationOnSeggerRtt rtt;
            services::SerialCommunicationMirror mirror{ uart, rtt };
            hal::SerialCommunication& console{ mirror };
#else
            hal::SerialCommunication& console{ uart };
#endif
            services::StreamWriterOnSerialCommunication::WithStorage<Resources::tracerBufferSize> streamWriterOnSerialCommunication{ console };
            infra::TextOutputStream::WithErrorPolicy tracerStream{ streamWriterOnSerialCommunication };
            services::TracerWithDateTime tracer{ tracerStream };
            services::TerminalWithCommandsImplBase<Resources::terminalCommandLength>::WithMaxQueueAndMaxHistory<Resources::terminalQueueSize, Resources::terminalHistorySize> terminal{ console, tracer };

            explicit TerminalAndTracer(hal::DmaStm& dma);
        };

        // Sequence order: the three phase currents, then CUR_TOTAL and VOLT_TOTAL for the board protection
        static constexpr std::size_t phaseCurrentChannels{ 5 };
        static constexpr std::size_t currentTotalSample{ 3 };
        static constexpr std::size_t powerSupplySample{ 4 };
        using PhaseCurrentAdc = AdcTriggeredByPwmStm<phaseCurrentChannels>;

        void ReconfigureAdc(SampleAndHold sampleAndHold);
        void ReconfigurePwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime);
        void ConfigureSamplingTrigger();
        void MonitorBoard(hal::AdcMultiChannel::Samples samples);
        void TripProtection(BoardProtectionReason reason);

        static CanBusAdapter::CanError ToAdapterError(hal::FdCanStm::Error error);

        void OnWatchdogExpired();

        // The EEPROM driver takes an unacknowledged transfer for a missing or busy device; the default hooks abort
        class EepromI2cStm
            : public hal::I2cStm
        {
        public:
            using hal::I2cStm::I2cStm;

        protected:
            void DeviceNotFound() override
            {}

            void BusError() override
            {}

            void ArbitrationLost() override
            {}
        };

        struct Peripherals
        {
            explicit Peripherals(PlatformFactoryImpl& platform);

            hal::OutputPin performance{ Pins::performance };
            Cortex cortex;
            hal::DmaStm dma;
            TerminalAndTracer terminalAndTracer{ dma };
            hal::AdcStm phaseCurrentConverter{ Peripheral::PhaseCurrentAdcIndex };
            hal::DmaStm::ReceiveStream phaseCurrentStream{ dma, hal::DmaChannelId{ Peripheral::DmaIndex, Peripheral::PhaseCurrentDmaChannel, Peripheral::phaseCurrentDmaRequest } };
            hal::AdcStm powerSupplyConverter{ Peripheral::PowerSupplyAdcIndex };
            hal::SynchronousAdcStm powerSupplyAdc;
            EepromI2cStm eepromI2c;
            drivers::Eeprom24xx eeprom{ eepromI2c, drivers::Eeprom24xx::Config{} };

            std::optional<AdcPhaseCurrentMeasurementImpl<PhaseCurrentAdc>> phaseCurrentAdc;
            std::optional<hal::PwmStm> pwm;
            std::optional<QuadratureEncoderDecoratorImpl<hal::SynchronousQuadratureEncoderStm>> encoder;
            std::optional<CanBusAdapterImpl<hal::FdCanStm::WithMaxRxBuffer<Resources::canReceiveBufferSize>>> canBus;

            struct PerformanceTrackerImpl
                : hal::PerformanceTracker
            {
                explicit PerformanceTrackerImpl(hal::cortex::DataWatchpointAndTrace& dwt, hal::OutputPin& pin);
                void Start() override;
                uint32_t ElapsedCycles() override;

                hal::cortex::DataWatchpointAndTrace& dwt;
                hal::OutputPin& pin;
            } performanceTrackerImpl{ cortex.dataWatchPointAndTrace, performance };
        };

    private:
        static void ConfigureBoardIdPins();
        static uint8_t ReadBoardIdPins();

        HalInitialization halInitialization;
        hal::cortex::InterruptTable::WithStorage<Resources::interruptTableSize> interruptTable;
        hal::GpioStm gpio;
        infra::Function<void()> onInitialized;
        FocLowPriorityInterruptAdapter pendSvLowPriorityInterrupt;
        ResetCause resetCause{ ResetCause::powerUp };
        uint8_t boardId{ 0 };
        [[no_unique_address]] CycleCounter cycleCounter;
        ControlLoopMetrics controlLoopMetrics;
        PlatformDiagnostics diagnostics{ controlLoopMetrics };
        volatile bool controlLoopEntered{ false };
        volatile bool protectionTripped{ false };
        foc::LowSideShuntReadout shuntReadout{ Peripheral::maxReadableDuty };
        infra::BoundedString::WithStorage<Resources::faultStatusSize> faultStatusString;
        hal::Hertz pwmBaseFrequency{ 20000 };
        foc::Radians encoderOffset{ 0.0f };
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        infra::Function<void(BoardProtectionReason)> onFaultCallback;
        std::optional<Peripherals> peripherals;
    };
}
