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

    void PositionCascade::SetSpeedTunings(const SpeedLoopTunings& tunings)
    {
        SetSpeedTuningsImpl(tunings);
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

    void PositionCascade::CommandSpeed(RadiansPerSecond speed)
    {
        speedCommand = speed;
    }

    hal::Hertz PositionCascade::SpeedCommandFrequency() const
    {
        return outerLoopFrequency;
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
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
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif
}
