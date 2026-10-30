#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "hal/interfaces/Eeprom.hpp"
#include "hal/interfaces/Pwm.hpp"
#include "infra/timer/Timer.hpp"
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

        StatusWithMessage ConfigurePwm(const infra::BoundedConstString& param);
        StatusWithMessage ConfigureAdc(const infra::BoundedConstString& param);
        StatusWithMessage ReadEncoder();
        StatusWithMessage Stop();
        void ProcessAdcSamples();
        void ReportAdcSamples();
        StatusWithMessage SetPwmDuty(const infra::BoundedConstString& param);
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
        StatusWithMessage StallEventLoop();

    private:
        // Leaves the command's response and trace time to reach the host before the event loop stops
        static constexpr infra::Duration eventLoopStallDelay{ std::chrono::milliseconds(100) };
        static constexpr std::size_t averageSampleSize = 100;
        static constexpr std::size_t adcReportLinesPerAction = 10;
        using QueueOfPhaseCurrents = infra::BoundedDeque<foc::PhaseCurrents>::WithMaxSize<averageSampleSize>;

        void StartAdc(PlatformFactory::SampleAndHold sampleAndHold);

        struct PeripheralConfiguration
        {
            hal::Hertz pwmFrequency{ 10000 };
            std::chrono::nanoseconds pwmDeadTime{ 500 };
            PlatformFactory::SampleAndHold sampleAndHold{ PlatformFactory::SampleAndHold::shortest };
        };

    private:
        const infra::BoundedVector<infra::BoundedConstString>::WithMaxSize<5> acceptedAdcValues{ { "shortest", "shorter", "medium", "longer", "longest" } };

        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        application::PlatformFactory& hardware;
        PeripheralConfiguration peripheralConfiguration;
        bool adcActive_{ false };
        bool adcReporting_{ false };
        bool canStarted = false;
        QueueOfPhaseCurrents queueOfPhaseCurrents;
        hal::Eeprom& eeprom;
        infra::TimerSingleShot eventLoopStallTimer;
        std::array<uint8_t, 64> eepromBuffer{};
    };
}
