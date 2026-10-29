#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"

namespace foc
{
    PositionCascade::PositionCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency, uint32_t dutyUpdateDelayPeriods)
        : CascadeWithSpeedLoop(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency, dutyUpdateDelayPeriods)
        , outerLoopFrequency(lowPriorityFrequency)
    {
        GetLowPriorityInterrupt().Register([this]()
            {
                LowPriorityHandler();
            });
    }

    bool PositionCascade::Configure(const MotorModelParameters& parameters)
    {
        return ConfigureImpl(parameters);
    }

    bool PositionCascade::ConfigureMechanics(const MechanicalModelParameters& parameters)
    {
        auto withLimits = parameters;
        const bool speedOk = ConfigureMechanicsImpl(withLimits);
        const bool positionOk = positionLoop.Configure(withLimits);
        return speedOk && positionOk;
    }

    void PositionCascade::SetPoint(Radians point)
    {
        lastPositionSetPoint = point;
    }

    void PositionCascade::SetCurrentTunings(const CurrentLoopTunings& tunings)
    {
        SetCurrentTuningsImpl(tunings);
    }

    SelectResult PositionCascade::SetSpeedTunings(const SpeedLoopTunings& tunings)
    {
        return SetSpeedTuningsImpl(tunings);
    }

    SelectResult PositionCascade::SetPositionTunings(const PositionLoopTunings& tunings)
    {
        if (enabled)
            return SelectResult::busy;

        return positionLoop.TrySetTunings(tunings);
    }

    SelectResult PositionCascade::SelectCurrentAlgorithm(CurrentAlgorithm algorithm)
    {
        return SelectCurrentAlgorithmImpl(algorithm);
    }

    CurrentAlgorithm PositionCascade::ActiveCurrentAlgorithm() const
    {
        return ActiveCurrentAlgorithmImpl();
    }

    SelectResult PositionCascade::SelectSpeedAlgorithm(SpeedAlgorithm algorithm)
    {
        return SelectSpeedAlgorithmImpl(algorithm);
    }

    SpeedAlgorithm PositionCascade::ActiveSpeedAlgorithm() const
    {
        return ActiveSpeedAlgorithmImpl();
    }

    SelectResult PositionCascade::SelectPositionAlgorithm(PositionAlgorithm algorithm)
    {
        if (enabled)
            return SelectResult::busy;

        return positionLoop.Select(algorithm);
    }

    PositionAlgorithm PositionCascade::ActivePositionAlgorithm() const
    {
        return positionLoop.Active();
    }

    void PositionCascade::SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator)
    {
        SetOnlineMechanicalEstimatorImpl(estimator);
    }

    void PositionCascade::SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator)
    {
        SetOnlineElectricalEstimatorImpl(estimator);
    }

    void PositionCascade::Enable()
    {
        speedCommandActive = false;
        positionLoop.Reset();
        EnableSpeedLoop();
        SetPoint(lastPositionSetPoint);
        enabled = true;
    }

    void PositionCascade::Disable()
    {
        enabled = false;
        speedCommandActive = false;
        DisableSpeedLoop();
    }

    void PositionCascade::EnableSpeedCommand()
    {
        speedCommand = RadiansPerSecond{ 0.0f };
        speedCommandActive = true;
        EnableSpeedLoop();
        enabled = true;
    }

    void PositionCascade::DisableSpeedCommand()
    {
        enabled = false;
        speedCommandActive = false;
        DisableSpeedLoop();
    }

    void PositionCascade::EnableCurrentCommand()
    {
        speedCommandActive = false;
        EnableCurrentCommandImpl();
        enabled = true;
    }

    void PositionCascade::DisableCurrentCommand()
    {
        Disable();
    }

    void PositionCascade::CommandCurrent(Ampere quadrature)
    {
        CommandCurrentImpl(quadrature);
    }

    Volts PositionCascade::QuadratureVoltage() const
    {
        return QuadratureVoltageImpl();
    }

    void PositionCascade::CommandSpeed(RadiansPerSecond speed)
    {
        speedCommand = speed;
    }

    hal::Hertz PositionCascade::SpeedCommandFrequency() const
    {
        return outerLoopFrequency;
    }

    MotionObservation PositionCascade::ObserveMotion() const
    {
        auto observation = ObserveSpeedLoopMotion();
        observation.positionError = Radians{ WrappedPositionError(lastPositionSetPoint, Radians{ CurrentMechanicalAngle() }) };

        // A law that commands the current leaves the speed reference wherever the speed loop's last user put it
        if (commandsCurrent)
            observation.demandedSpeed = RadiansPerSecond{ 0.0f };

        return observation;
    }

    void PositionCascade::LowPriorityHandler()
    {
        auto mechanicalSpeed = MeasureMechanicalSpeed();

        if (HoldCommandedCurrent())
        {
            commandsCurrent = true;
            return;
        }

        if (speedCommandActive)
        {
            commandsCurrent = false;
            SetSpeedReference(speedCommand);
            RunSpeedLoop(mechanicalSpeed);
            UpdateOnlineMechanicalEstimator();
            UpdateOnlineElectricalEstimator();
            return;
        }

        auto command = positionLoop.Compute(PositionControlContext{ Radians{ CurrentMechanicalAngle() }, lastPositionSetPoint, RadiansPerSecond{ mechanicalSpeed } });
        commandsCurrent = command.kind != PositionOutputKind::speedReference;

        if (command.kind == PositionOutputKind::speedReference)
        {
            SetSpeedReference(RadiansPerSecond{ command.value });
            RunSpeedLoop(mechanicalSpeed);
        }
        else
            SetDirectCurrentReference(command.value);

        UpdateOnlineMechanicalEstimator();
        UpdateOnlineElectricalEstimator();
    }

    PhasePwmDutyCycles PositionCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }

    void PositionCascade::LoadCoggingMap(infra::MemoryRange<const int8_t> torque, float torqueScale, NewtonMeter torqueConstant)
    {
        LoadCoggingMapImpl(torque, torqueScale, torqueConstant);
    }

    void PositionCascade::ClearCoggingMap()
    {
        ClearCoggingMapImpl();
    }
}
