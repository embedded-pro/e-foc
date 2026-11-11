#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"

namespace foc
{
    PositionCascade::PositionCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency, uint32_t dutyUpdateDelayPeriods)
        : SpeedLoopDrive(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency, dutyUpdateDelayPeriods)
        , outerLoopFrequency(lowPriorityFrequency)
        , profile(lowPriorityFrequency)
    {
        GetLowPriorityInterrupt().Register([this]()
            {
                LowPriorityHandler();
            });
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
        profile.SetTarget(point);
    }

    void PositionCascade::LimitTravel(Radians minimum, Radians maximum)
    {
        travelLimits = TravelLimits{ minimum, maximum };
    }

    void PositionCascade::ProfileMotion(RadiansPerSecond velocity, RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration, RadiansPerSecondCubed)
    {
        profile.Limit(velocity, acceleration, deceleration);
    }

    SelectResult PositionCascade::SetPositionTunings(const PositionLoopTunings& tunings)
    {
        if (enabled)
            return SelectResult::busy;

        return positionLoop.TrySetTunings(tunings);
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

    void PositionCascade::Enable()
    {
        speedCommandActive = false;
        positionLoop.Reset();
        EnableSpeedLoop();
        profile.Release();
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
        Disable();
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
        observation.positionError = ErrorTo(profile.Reference());
        observation.targetError = ErrorTo(profile.Target());

        // A law that commands the current leaves the speed reference wherever the speed loop's last user put it
        if (commandsCurrent)
            observation.demandedSpeed = RadiansPerSecond{ 0.0f };

        return observation;
    }

    Radians PositionCascade::ErrorTo(Radians point) const
    {
        const Radians measured{ CurrentMechanicalAngle() };
        const auto limits = travelLimits;
        if (!limits.IsOn())
            return Radians{ PositionDifference(point, measured, false) };

        return Radians{ PositionDifference(limits.Hold(point), measured, true) };
    }

    void PositionCascade::LowPriorityHandler()
    {
        auto mechanicalSpeed = MeasureMechanicalSpeed();

        if (HoldCommandedCurrent())
        {
            profile.Release();
            commandsCurrent = true;
            return;
        }

        if (speedCommandActive)
        {
            profile.Release();
            commandsCurrent = false;
            SetSpeedReference(speedCommand);
            RunSpeedLoop(mechanicalSpeed);
            UpdateOnlineMechanicalEstimator();
            UpdateOnlineElectricalEstimator();
            return;
        }

        const auto limits = travelLimits;
        const auto reference = profile.Advance(limits, Radians{ CurrentMechanicalAngle() }, RadiansPerSecond{ mechanicalSpeed });
        auto command = positionLoop.Compute(ControlContext(limits, reference, mechanicalSpeed));
        commandsCurrent = command.kind != PositionOutputKind::speedReference;

        if (command.kind == PositionOutputKind::speedReference)
        {
            SetSpeedReference(RadiansPerSecond{ command.value });
            RunSpeedLoop(mechanicalSpeed, reference.acceleration.Value());
        }
        else
            SetDirectCurrentReference(command.value, reference.acceleration.Value());

        UpdateOnlineMechanicalEstimator();
        UpdateOnlineElectricalEstimator();
    }

    PhasePwmDutyCycles PositionCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }

}
