#pragma once

#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "core/foc/implementations/WithAutomaticSpeedPidGains.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "hal/interfaces/Eeprom.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "infra/util/BoundedDeque.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace application
{
    class TerminalInteractor
    {
    public:
        TerminalInteractor(services::TerminalWithStorage& terminal, application::PlatformFactory& hardware);

    private:
        using StatusWithMessage = services::TerminalWithStorage::StatusWithMessage;
        using CommandInfo = services::TerminalWithStorage::CommandInfo;
        using CommandHandler = infra::Function<void(const infra::BoundedConstString&)>;

        struct GuardedCommand
        {
            GuardedCommand(CommandHandler h, bool allowed)
                : handler(std::move(h)), allowedWhileSpinning(allowed) {}
            GuardedCommand(GuardedCommand&&) noexcept = default;
            GuardedCommand& operator=(GuardedCommand&&) noexcept = default;

            CommandHandler handler;
            bool allowedWhileSpinning{ false };
        };

        // Registers a handler that is rejected while the motor is spinning, unless the command is safe to run then.
        void AddCommand(const CommandInfo& info, const CommandHandler& handler, bool allowedWhileSpinning = false);

        struct IdentificationResults
        {
            services::ElectricalParametersIdentification::ResistanceInductanceResult rl{
                foc::Ohm{ 0.0f }, foc::MilliHenry{ 0.0f }, foc::Volts{ 0.0f }, 0.0f
            };
            std::size_t polePairs{ 0 };
        };

        StatusWithMessage ConfigurePwm(const infra::BoundedConstString& param);
        StatusWithMessage ConfigureAdc(const infra::BoundedConstString& param);
        StatusWithMessage SimulateFoc(const infra::BoundedConstString& param);
        StatusWithMessage ConfigurePid(const infra::BoundedConstString& param);
        StatusWithMessage ReadEncoder();
        StatusWithMessage Stop();
        void ProcessAdcSamples();
        StatusWithMessage SetPwmDuty(const infra::BoundedConstString& param);
        StatusWithMessage IdentifyElectricalParameters(const infra::BoundedConstString& param);
        StatusWithMessage AlignMotor(const infra::BoundedConstString& param);
        StatusWithMessage RunSpeedFoc(const infra::BoundedConstString& param);
        StatusWithMessage ReportSpeedEstimates();
        StatusWithMessage CanStart(const infra::BoundedConstString& param);
        StatusWithMessage CanStop();
        StatusWithMessage CanSend(const infra::BoundedConstString& param);
        StatusWithMessage CanListen();
        void EepromWrite(const infra::BoundedConstString& param);
        void EepromRead(const infra::BoundedConstString& param);
        void EepromErase();
        StatusWithMessage ResetDevice();
        StatusWithMessage GetResetCauseStatus();
        StatusWithMessage GetFaultStatus();
        StatusWithMessage ForceHardfault();
        void RunPolePairEstimation();
        void ReportIdentificationResults();

    private:
        static constexpr std::size_t averageSampleSize = 100;
        using QueueOfPhaseCurrents = infra::BoundedDeque<foc::PhaseCurrents>::WithMaxSize<averageSampleSize>;

        // Rough bench defaults; the online estimators refine these live — retune per rig.
        static constexpr float defaultInertiaValue{ 7.5e-6f };  // kg*m^2
        static constexpr float defaultFrictionValue{ 2.0e-5f }; // N*m*s/rad
        static constexpr float defaultSpeedBandwidthRadPerSec{ 100.0f };
        static constexpr float currentLoopNyquistFactor{ 0.1f };

        void StartAdc(PlatformFactory::SampleAndHold sampleAndHold);
        bool IsAdcBufferPopulated() const;
        void RunFocSimulation(foc::PhaseCurrents input, foc::Radians angle);

        struct PwmAdcConfig
        {
            hal::Hertz frequency{ 10000 };
            std::chrono::nanoseconds deadTime{ 500 };
            PlatformFactory::SampleAndHold sah{ PlatformFactory::SampleAndHold::shortest };
            bool active{ false };
        };

        struct EepromData
        {
            std::array<uint8_t, 64> buffer{};
            uint32_t currentReadSize{ 0 };
        };

        struct MotorPidTunings
        {
            controllers::PidTunings<float> speed;
            controllers::PidTunings<float> dq;
        };

        struct RuntimeState
        {
            std::optional<std::size_t> polePairs{ 0 };
            bool speedActive{ false };
            bool canStarted{ false };
        };

        struct MotorIdentState
        {
            std::optional<IdentificationResults> results;
            bool aligned{ false };
            services::ElectricalParametersIdentification::PolePairsConfig pendingPolePairsConfig;
        };

    private:
        const infra::BoundedVector<infra::BoundedConstString>::WithMaxSize<5> acceptedAdcValues{ { "shortest", "shorter", "medium", "longer", "longest" } };
        infra::BoundedVector<GuardedCommand>::WithMaxSize<24> guardedCommands;

        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        application::PlatformFactory& hardware;
        PwmAdcConfig pwmAdcConfig;
        QueueOfPhaseCurrents queueOfPhaseCurrents;
        hal::PerformanceTracker& performanceTimer;
        foc::Volts Vdc;
        MotorPidTunings pidTunings;
        RuntimeState runtimeState;
        // foc's current-PID dt is fixed at this rate, so the live loop always reconfigures back to it.
        hal::Hertz baseFrequency_{ hardware.BaseFrequency() };
        foc::FocSpeedImpl foc;
        services::RealTimeFrictionAndInertiaEstimator onlineMechEstimator;
        services::RealTimeResistanceAndInductanceEstimator onlineElecEstimator;
        hal::Eeprom& eeprom;
        EepromData eepromData;
        services::ElectricalParametersIdentificationImpl electricalIdent;
        services::MotorAlignmentImpl motorAlignment;
        MotorIdentState motorIdentState;
    };
}
