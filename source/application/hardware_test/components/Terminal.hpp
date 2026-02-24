#pragma once

#include "hal/interfaces/AdcMultiChannel.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/util/BoundedDeque.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include "source/foc/instantiations/FocImpl.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/hardware/HardwareFactory.hpp"

namespace application
{
    class TerminalInteractor
    {
    public:
        TerminalInteractor(services::TerminalWithStorage& terminal, application::HardwareFactory& hardware);

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

    private:
        static constexpr std::size_t averageSampleSize = 100;
        using QueueOfPhaseCurrents = infra::BoundedDeque<foc::PhaseCurrents>::WithMaxSize<averageSampleSize>;

        void StartAdc(HardwareFactory::SampleAndHold sampleAndHold);
        bool IsAdcBufferPopulated() const;
        void RunFocSimulation(foc::PhaseCurrents input, foc::Radians angle);

    private:
        const infra::BoundedVector<infra::BoundedConstString>::WithMaxSize<5> acceptedAdcValues{ { "shortest", "shorter", "medium", "longer", "longest" } };

        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        infra::DelayedProxyCreator<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds, hal::Hertz)> pwmCreator;
        infra::DelayedProxyCreator<AdcPhaseCurrentMeasurement, void(HardwareFactory::SampleAndHold)> adcCreator;
        infra::DelayedProxyCreator<QuadratureEncoderDecorator, void()> encoderCreator;
        QueueOfPhaseCurrents queueOfPhaseCurrents;
        hal::PerformanceTracker& performanceTimer;
        foc::Volts Vdc;
        hal::Hertz systemClock;
        controllers::PidTunings<float> speedPidTunings;
        controllers::PidTunings<float> dqPidTunings;
        std::optional<std::size_t> polePairs = 0;
        foc::FocSpeedImpl foc;
    };
}
