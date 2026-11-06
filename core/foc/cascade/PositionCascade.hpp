#pragma once

#include "core/foc/cascade/CascadeWithSpeedLoop.hpp"
#include "core/foc/math/TravelLimits.hpp"
#include "core/foc/position_loop/PositionControllerSelector.hpp"
#include "foc/interfaces/Foc.hpp"

namespace foc
{
    class PositionCascade
        : public FocPosition
        , public SpeedCommandable
        , public CurrentCommandable
        , public CoggingCompensable
        , public CurrentLimitable
        , public BusVoltageTrackable
        , public PositionLimitable
        , public QuickStoppable
        , protected CascadeWithSpeedLoop
    {
    public:
        explicit PositionCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency = hal::Hertz{ 1000 }, uint32_t dutyUpdateDelayPeriods = 0);

        bool Configure(const MotorModelParameters& parameters) override;
        bool ConfigureMechanics(const MechanicalModelParameters& parameters) override;
        void SetPoint(Radians point) override;
        void SetCurrentTunings(const CurrentLoopTunings& tunings) override;
        SelectResult SetSpeedTunings(const SpeedLoopTunings& tunings) override;
        SelectResult SetPositionTunings(const PositionLoopTunings& tunings) override;
        SelectResult SelectCurrentAlgorithm(CurrentAlgorithm algorithm) override;
        CurrentAlgorithm ActiveCurrentAlgorithm() const override;
        SelectResult SelectSpeedAlgorithm(SpeedAlgorithm algorithm) override;
        SpeedAlgorithm ActiveSpeedAlgorithm() const override;
        SelectResult SelectPositionAlgorithm(PositionAlgorithm algorithm) override;
        PositionAlgorithm ActivePositionAlgorithm() const override;
        void SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator) override;
        void SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator) override;
        void Enable() override;
        void Disable() override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) final;
        MotionObservation ObserveMotion() const override;

        void EnableSpeedCommand() override;
        void DisableSpeedCommand() override;
        void CommandSpeed(RadiansPerSecond speed) override;
        void QuickStop(RadiansPerSecondSquared deceleration) override;
        hal::Hertz SpeedCommandFrequency() const override;

        void EnableCurrentCommand() override;
        void DisableCurrentCommand() override;
        void CommandCurrent(Ampere quadrature) override;
        Volts QuadratureVoltage() const override;

        void LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant) override;
        void ClearCoggingMap() override;
        void LimitCurrent(Ampere limit) override;
        void TrackBusVoltage(Volts measured) override;
        void LimitTravel(Radians minimum, Radians maximum) override;
        using CascadeWithSpeedLoop::SetCurrentStream;

    private:
        void LowPriorityHandler();

        OPTIMIZE_FOR_SPEED PositionControlContext ControlContext(const TravelLimits& limits, float mechanicalSpeed) const
        {
            const Radians measured{ CurrentMechanicalAngle() };
            if (!limits.IsOn())
                return { measured, lastPositionSetPoint, RadiansPerSecond{ mechanicalSpeed } };

            return { measured, limits.Hold(lastPositionSetPoint), RadiansPerSecond{ mechanicalSpeed }, true };
        }

        PositionControllerSelector positionLoop;
        hal::Hertz outerLoopFrequency;
        Radians lastPositionSetPoint{ 0.0f };
        // Written by the event loop; each outer-loop tick works on one copy, so its IsOn check guards its clamp.
        TravelLimits travelLimits;
        RadiansPerSecond speedCommand{ 0.0f };
        bool speedCommandActive{ false };
        volatile bool commandsCurrent{ false };
        bool enabled{ false };
    };
}
