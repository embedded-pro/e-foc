#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/position_loop/PositionPlantModel.hpp"

namespace foc
{
    PositionCascade::PositionCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, CurrentStream& currentStream, hal::Hertz lowPriorityFrequency, uint32_t dutyUpdateDelayPeriods)
        : SpeedLoopDrive(maxCurrent, baseFrequency, lowPriorityInterrupt, currentStream, lowPriorityFrequency, dutyUpdateDelayPeriods)
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

    void PositionCascade::PositionCircular(bool circular)
    {
        requestedCircular = circular;
    }

    void PositionCascade::SetPositionTracker(TurnHandover& tracker)
    {
        this->tracker = &tracker;
    }

    void PositionCascade::ProfileMotion(RadiansPerSecond velocity, RadiansPerSecondSquared acceleration, RadiansPerSecondSquared deceleration, RadiansPerSecondCubed jerk)
    {
        profile.Limit(velocity, acceleration, deceleration, jerk);
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
        TakeOverTurns();
        speedCommandActive = false;
        positionLoop.Reset();
        EnableSpeedLoop();
        profile.Release();
        enabled = true;
    }

    void PositionCascade::Disable()
    {
        HandBackTurns();
        enabled = false;
        speedCommandActive = false;
        DisableSpeedLoop();
    }

    void PositionCascade::EnableSpeedCommand()
    {
        TakeOverTurns();
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
        TakeOverTurns();
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

    void PositionCascade::QuickStop(RadiansPerSecondSquared deceleration)
    {
        BeginQuickStop(deceleration);
    }

    hal::Hertz PositionCascade::SpeedCommandFrequency() const
    {
        return outerLoopFrequency;
    }

    MotionObservation PositionCascade::ObserveMotion() const
    {
        auto observation = ObserveSpeedLoopMotion();
        const auto measured = ObservedPosition();
        observation.positionError = ErrorTo(profile.Reference(), measured);
        observation.targetError = ErrorTo(profile.Target(), measured);
        observation.measuredPosition = measured;

        // A law that commands the current leaves the speed reference wherever the speed loop's last user put it
        if (commandsCurrent)
            observation.demandedSpeed = RadiansPerSecond{ 0.0f };

        return observation;
    }

    void PositionCascade::TakeOverTurns()
    {
        if (enabled)
            return;

        circular = requestedCircular;
        const auto held = tracker != nullptr ? tracker->Hold() : std::nullopt;
        if (held)
        {
            turns.Seed(*held);
            publishedTurns.Write(*held);
        }
        else
        {
            turns.Restart();
            publishedTurns.Clear();
        }
    }

    void PositionCascade::HandBackTurns()
    {
        if (enabled && tracker != nullptr)
            tracker->Release();
    }

    TravelLimits PositionCascade::CurrentLimits() const
    {
        auto limits = travelLimits;
        limits.circular = circular;
        return limits;
    }

    Radians PositionCascade::ObservedPosition() const
    {
        const auto angle = CurrentMechanicalAngle();
        if (circular)
            return Radians{ angle };

        TurnCounter counter;
        if (const auto last = publishedTurns.Read())
            counter.Seed(*last);
        counter.Track(angle);
        return Radians{ PositionOf(counter.Count()) };
    }

    Radians PositionCascade::ErrorTo(Radians point, Radians measured) const
    {
        const auto limits = CurrentLimits();
        if (!limits.IsOn())
            return Radians{ PositionDifference(point, measured, !limits.Folds()) };

        return Radians{ PositionDifference(limits.Hold(point), measured, true) };
    }

    void PositionCascade::LowPriorityHandler()
    {
        auto mechanicalSpeed = MeasureMechanicalSpeed();
        CountTurns();

        if (HoldCommandedCurrent())
        {
            profile.Release();
            commandsCurrent = true;
            return;
        }

        if (RunQuickStop(mechanicalSpeed))
        {
            commandsCurrent = false;
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

        const auto limits = CurrentLimits();
        const auto reference = profile.Advance(limits, MeasuredPosition(), RadiansPerSecond{ mechanicalSpeed });
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
