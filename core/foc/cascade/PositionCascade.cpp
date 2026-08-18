#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/cascade/PositionCascade.hpp"

namespace foc
{
    PositionCascade::PositionCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : CascadeWithSpeedLoop(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency)
        , outerLoopFrequency(lowPriorityFrequency)
    {
        GetLowPriorityInterrupt().Register([this]()
            {
                LowPriorityHandler();
            });
    }

    void PositionCascade::Configure(const MotorModelParameters& parameters)
    {
        ConfigureImpl(parameters);
    }

    void PositionCascade::ConfigureMechanics(const MechanicalModelParameters& parameters)
    {
        positionLoop.Configure(ConfigureMechanicsImpl(parameters));
    }

    void PositionCascade::SetPoint(Radians point)
    {
        lastPositionSetPoint = point;
    }

    void PositionCascade::SetCurrentTunings(const CurrentLoopTunings& tunings)
    {
        SetCurrentTuningsImpl(tunings);
    }

    void PositionCascade::SetSpeedTunings(const SpeedLoopTunings& tunings)
    {
        SetSpeedTuningsImpl(tunings);
    }

    SelectResult PositionCascade::SetPositionTunings(const PositionLoopTunings& tunings)
    {
        // Retuning redesigns the active law, which cannot be done underneath a running motor
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

    void PositionCascade::CommandSpeed(RadiansPerSecond speed)
    {
        speedCommand = speed;
    }

    hal::Hertz PositionCascade::SpeedCommandFrequency() const
    {
        return outerLoopFrequency;
    }

    OPTIMIZE_FOR_SPEED
    void PositionCascade::LowPriorityHandler()
    {
        auto mechanicalSpeed = MeasureMechanicalSpeed();

        if (speedCommandActive)
        {
            SetSpeedReference(speedCommand);
            RunSpeedLoop(mechanicalSpeed);
            UpdateOnlineMechanicalEstimator(mechanicalSpeed);
            UpdateOnlineElectricalEstimator(mechanicalSpeed * PolePairs());
            return;
        }

        auto command = positionLoop.Compute(PositionControlContext{ Radians{ CurrentMechanicalAngle() }, lastPositionSetPoint, RadiansPerSecond{ mechanicalSpeed } });

        // A law that sizes the current itself owns the mechanical response, so the speed loop steps aside
        if (command.kind == PositionOutputKind::speedReference)
        {
            SetSpeedReference(RadiansPerSecond{ command.value });
            RunSpeedLoop(mechanicalSpeed);
        }
        else
            SetDirectCurrentReference(command.value);

        UpdateOnlineMechanicalEstimator(mechanicalSpeed);
        UpdateOnlineElectricalEstimator(mechanicalSpeed * PolePairs());
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles PositionCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }
}
