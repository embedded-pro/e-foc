#pragma once

#include "core/foc/implementations/FocWithSpeedLoop.hpp"

namespace foc
{
    class FocSpeedImpl
        : public FocSpeed
        , protected FocWithSpeedLoop
    {
    public:
        explicit FocSpeedImpl(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 });

        void SetPolePairs(std::size_t polePairs) override;
        void SetPoint(RadiansPerSecond point) override;
        void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings) override;
        void SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning) override;
        void Enable() override;
        void Disable() override;
        hal::Hertz OuterLoopFrequency() const override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;

    private:
        void LowPriorityHandler();

    private:
        RadiansPerSecond lastSpeedSetPoint{ 0.0f };
        hal::Hertz outerLoopFrequency;
    };
}
