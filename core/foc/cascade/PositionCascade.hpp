#pragma once

#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"

namespace foc
{
    class PositionCascade
        : public FocPosition
        , protected CascadeWithSpeedLoop
    {
    public:
        explicit PositionCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 });

        void Configure(const MotorModelParameters& parameters) override;
        void ConfigureMechanics(const MechanicalModelParameters& parameters) override;
        void SetPoint(Radians point) override;
        void SetCurrentTunings(const CurrentLoopTunings& tunings) override;
        void SetSpeedTunings(const SpeedLoopTunings& tunings) override;
        void SetPositionTunings(const PositionLoopTunings& tunings) override;
        void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) override;
        void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;

        using CascadeWithSpeedLoop::CurrentLoop;
        using CascadeWithSpeedLoop::SpeedLoop;

    private:
        void LowPriorityHandler();

        // Proportional position loop: the speed reference is the position error times the loop bandwidth.
        float positionGain{ PositionLoopTunings{}.bandwidth };
        Radians lastPositionSetPoint{ 0.0f };
    };
}
