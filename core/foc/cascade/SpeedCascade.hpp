#pragma once

#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
#include "foc/interfaces/Foc.hpp"

namespace foc
{
    class SpeedCascade
        : public FocSpeed
        , public SpeedCommandable
        , public CurrentCommandable
        , public CoggingCompensable
        , protected CascadeWithSpeedLoop
    {
    public:
        explicit SpeedCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 }, uint32_t dutyUpdateDelayPeriods = 0);

        bool Configure(const MotorModelParameters& parameters) override;
        bool ConfigureMechanics(const MechanicalModelParameters& parameters) override;
        void SetPoint(RadiansPerSecond point) override;
        void SetCurrentTunings(const CurrentLoopTunings& tunings) override;
        SelectResult SetSpeedTunings(const SpeedLoopTunings& tunings) override;
        SelectResult SelectCurrentAlgorithm(CurrentAlgorithm algorithm) override;
        CurrentAlgorithm ActiveCurrentAlgorithm() const override;
        SelectResult SelectSpeedAlgorithm(SpeedAlgorithm algorithm) override;
        SpeedAlgorithm ActiveSpeedAlgorithm() const override;
        void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) override;
        void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) override;
        void Enable() override;
        void Disable() override;
        hal::Hertz OuterLoopFrequency() const override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) final;
        MotionObservation ObserveMotion() const override;

        void EnableSpeedCommand() override;
        void DisableSpeedCommand() override;
        void CommandSpeed(RadiansPerSecond speed) override;
        hal::Hertz SpeedCommandFrequency() const override;

        void EnableCurrentCommand() override;
        void DisableCurrentCommand() override;
        void CommandCurrent(Ampere quadrature) override;
        Volts QuadratureVoltage() const override;

        void LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant) override;
        void ClearCoggingMap() override;

    private:
        void LowPriorityHandler();

    private:
        RadiansPerSecond lastSpeedSetPoint{ 0.0f };
        hal::Hertz outerLoopFrequency;
    };
}
