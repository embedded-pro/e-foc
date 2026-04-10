#include "source/platform_abstraction/MotorFieldOrientedControllerAdapter.hpp"
#include <chrono>

namespace application
{
    HardwareAdapter::HardwareAdapter(HardwareFactory& hardware)
        : adcMultiChannelCreator{ hardware.AdcMultiChannelCreator(), HardwareFactory::SampleAndHold::shorter }
        , synchronousThreeChannelsPwmCreator{ hardware.SynchronousThreeChannelsPwmCreator(), pwmDeadTime, pwmBaseFrequency }
        , synchronousQuadratureEncoderCreator(hardware.SynchronousQuadratureEncoderCreator())
    {
    }

    void HardwareAdapter::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        onPhaseCurrentsReady = onDone;
        synchronousThreeChannelsPwmCreator->SetBaseFrequency(baseFrequency);
        adcMultiChannelCreator->Measure([this](auto phaseA, auto phaseB, auto phaseC)
            {
                onPhaseCurrentsReady(foc::PhaseCurrents{ phaseA, phaseB, phaseC });
            });
    }

    void HardwareAdapter::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        synchronousThreeChannelsPwmCreator->Start(dutyPhases.a, dutyPhases.b, dutyPhases.c);
    }

    void HardwareAdapter::Start()
    {
        synchronousThreeChannelsPwmCreator->Start(hal::Percent{ 1 }, hal::Percent{ 1 }, hal::Percent{ 1 });
    }

    void HardwareAdapter::Stop()
    {
        synchronousThreeChannelsPwmCreator->Stop();
    }

    hal::Hertz HardwareAdapter::BaseFrequency() const
    {
        return pwmBaseFrequency;
    }

    foc::Radians HardwareAdapter::Read()
    {
        const auto encoderReading = synchronousQuadratureEncoderCreator->Read();
        const auto offset = encoderOffset;
        return encoderReading - offset;
    }

    void HardwareAdapter::Set(foc::Radians value)
    {
        const auto encoderReading = synchronousQuadratureEncoderCreator->Read();
        encoderOffset = encoderReading - value;
    }

    void HardwareAdapter::SetZero()
    {
        encoderOffset = synchronousQuadratureEncoderCreator->Read();
    }
}
