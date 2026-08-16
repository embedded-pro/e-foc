#pragma once

#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"

namespace foc
{
    class SpeedCascade
        : public FocSpeed
        , protected CascadeWithSpeedLoop
    {
    public:
        explicit SpeedCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 });

        void SetPolePairs(std::size_t polePairs) override;
        void SetPoint(RadiansPerSecond point) override;
        void SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings) override;
        void SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning) override;
        void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) override;
        void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) override;
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
