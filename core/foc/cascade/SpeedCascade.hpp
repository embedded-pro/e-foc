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

        void Configure(const MotorModelParameters& parameters) override;
        void ConfigureMechanics(const MechanicalModelParameters& parameters) override;
        void SetPoint(RadiansPerSecond point) override;
        void SetCurrentTunings(const CurrentLoopTunings& tunings) override;
        void SetSpeedTunings(const SpeedLoopTunings& tunings) override;
        void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) override;
        void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) override;
        void Enable() override;
        void Disable() override;
        hal::Hertz OuterLoopFrequency() const override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;

        using CascadeWithSpeedLoop::CurrentLoop;
        using CascadeWithSpeedLoop::SpeedLoop;

    private:
        void LowPriorityHandler();

    private:
        RadiansPerSecond lastSpeedSetPoint{ 0.0f };
        hal::Hertz outerLoopFrequency;
    };
}
