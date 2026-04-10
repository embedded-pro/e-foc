#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"

namespace application
{
    class PlatformAdapter
        : public foc::ThreePhaseInverter
        , public foc::Encoder
    {
    public:
        explicit PlatformAdapter(PlatformFactory& hardware);

        // Implementation of ThreePhaseInverter
        OPTIMIZE_FOR_SPEED
        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        OPTIMIZE_FOR_SPEED
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;

        // Implementation of Encoder
        OPTIMIZE_FOR_SPEED
        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        foc::Radians encoderOffset{ 0.0f };
        hal::Hertz pwmBaseFrequency{ 10000 };
        std::chrono::nanoseconds pwmDeadTime{ 500 };
        infra::ProxyCreator<AdcPhaseCurrentMeasurement, void(PlatformFactory::SampleAndHold)> adcMultiChannelCreator;
        infra::ProxyCreator<hal::SynchronousThreeChannelsPwm, void(std::chrono::nanoseconds deadTime, hal::Hertz frequency)> synchronousThreeChannelsPwmCreator;
        infra::ProxyCreator<QuadratureEncoderDecorator, void()> synchronousQuadratureEncoderCreator;
        infra::Function<void(foc::PhaseCurrents currentPhases)> onPhaseCurrentsReady;
    };
}
