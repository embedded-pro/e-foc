#pragma once

#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
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

        StatusWithMessage ConfigurePwm(const infra::BoundedConstString& param);
        StatusWithMessage ConfigureAdc(const infra::BoundedConstString& param);
        StatusWithMessage SimulateFoc(const infra::BoundedConstString& param);
        StatusWithMessage ConfigurePid(const infra::BoundedConstString& param);
        StatusWithMessage ReadEncoder();
        StatusWithMessage Stop();
        void ProcessAdcSamples();
        StatusWithMessage SetPwmDuty(const infra::BoundedConstString& param);
        StatusWithMessage SetMotorParameters(const infra::BoundedConstString& param);
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

    private:
        static constexpr std::size_t averageSampleSize = 100;
        using QueueOfPhaseCurrents = infra::BoundedDeque<foc::PhaseCurrents>::WithMaxSize<averageSampleSize>;

        void StartAdc(PlatformFactory::SampleAndHold sampleAndHold);
        bool IsAdcBufferPopulated() const;
        void RunFocSimulation(foc::PhaseCurrents input, foc::Radians angle);

    private:
        const infra::BoundedVector<infra::BoundedConstString>::WithMaxSize<5> acceptedAdcValues{ { "shortest", "shorter", "medium", "longer", "longest" } };

        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        application::PlatformFactory& hardware;
        hal::Hertz currentPwmFrequency_{ 10000 };
        std::chrono::nanoseconds currentPwmDeadTime_{ 500 };
        PlatformFactory::SampleAndHold currentSah_{ PlatformFactory::SampleAndHold::shortest };
        bool adcActive_{ false };
        bool canStarted = false;
        QueueOfPhaseCurrents queueOfPhaseCurrents;
        hal::PerformanceTracker& performanceTimer;
        foc::Volts Vdc;
        hal::Hertz systemClock;
        controllers::PidTunings<float> speedPidTunings;
        controllers::PidTunings<float> dqPidTunings;
        std::optional<std::size_t> polePairs = 0;
        foc::FocSpeedImpl foc;
        hal::Eeprom& eeprom;
        std::array<uint8_t, 64> eepromBuffer{};
        uint32_t eepromCurrentReadSize{ 0 };
    };
}
