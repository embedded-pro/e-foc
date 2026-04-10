#include "core/platform_abstraction/MotorFieldOrientedControllerAdapter.hpp"
#include <chrono>

namespace application
{
    PlatformAdapter::PlatformAdapter(PlatformFactory& hardware)
        : platformFactory{ hardware }
        , adcMultiChannelCreator{ hardware.AdcMultiChannelCreator(), PlatformFactory::SampleAndHold::shorter }
        , synchronousThreeChannelsPwmCreator{ hardware.SynchronousThreeChannelsPwmCreator(), pwmDeadTime, pwmBaseFrequency }
        , synchronousQuadratureEncoderCreator(hardware.SynchronousQuadratureEncoderCreator())
    {
    }

    void PlatformAdapter::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        onPhaseCurrentsReady = onDone;
        synchronousThreeChannelsPwmCreator->SetBaseFrequency(baseFrequency);
        adcMultiChannelCreator->Measure([this](auto phaseA, auto phaseB, auto phaseC)
            {
                onPhaseCurrentsReady(foc::PhaseCurrents{ phaseA, phaseB, phaseC });
            });
    }

    void PlatformAdapter::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        synchronousThreeChannelsPwmCreator->Start(dutyPhases.a, dutyPhases.b, dutyPhases.c);
    }

    void PlatformAdapter::Start()
    {
        synchronousThreeChannelsPwmCreator->Start(hal::Percent{ 1 }, hal::Percent{ 1 }, hal::Percent{ 1 });
    }

    void PlatformAdapter::Stop()
    {
        synchronousThreeChannelsPwmCreator->Stop();
    }

    hal::Hertz PlatformAdapter::BaseFrequency() const
    {
        return pwmBaseFrequency;
    }

    foc::Ampere PlatformAdapter::MaxCurrentSupported() const
    {
        return platformFactory.MaxCurrentSupported();
    }

    foc::Radians PlatformAdapter::Read()
    {
        const auto encoderReading = synchronousQuadratureEncoderCreator->Read();
        const auto offset = encoderOffset;
        return encoderReading - offset;
    }

    void PlatformAdapter::Set(foc::Radians value)
    {
        const auto encoderReading = synchronousQuadratureEncoderCreator->Read();
        encoderOffset = encoderReading - value;
    }

    void PlatformAdapter::SetZero()
    {
        encoderOffset = synchronousQuadratureEncoderCreator->Read();
    }
}
