#include "core/foc/cascade/SpeedCascade.hpp"

namespace foc
{
    SpeedCascade::SpeedCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : CascadeWithSpeedLoop(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency)
        , outerLoopFrequency(lowPriorityFrequency)
    {
        GetLowPriorityInterrupt().Register([this]()
            {
                LowPriorityHandler();
            });
    }

    bool SpeedCascade::Configure(const MotorModelParameters& parameters)
    {
        return ConfigureImpl(parameters);
    }

    bool SpeedCascade::ConfigureMechanics(const MechanicalModelParameters& parameters)
    {
        auto withLimits = parameters;
        return ConfigureMechanicsImpl(withLimits);
    }

    void SpeedCascade::SetPoint(RadiansPerSecond point)
    {
        lastSpeedSetPoint = point;
        SetSpeedReference(point);
    }

    void SpeedCascade::SetCurrentTunings(const CurrentLoopTunings& tunings)
    {
        SetCurrentTuningsImpl(tunings);
    }

    void SpeedCascade::SetSpeedTunings(const SpeedLoopTunings& tunings)
    {
        SetSpeedTuningsImpl(tunings);
    }

    SelectResult SpeedCascade::SelectCurrentAlgorithm(CurrentAlgorithm algorithm)
    {
        return SelectCurrentAlgorithmImpl(algorithm);
    }

    CurrentAlgorithm SpeedCascade::ActiveCurrentAlgorithm() const
    {
        return ActiveCurrentAlgorithmImpl();
    }

    SelectResult SpeedCascade::SelectSpeedAlgorithm(SpeedAlgorithm algorithm)
    {
        return SelectSpeedAlgorithmImpl(algorithm);
    }

    SpeedAlgorithm SpeedCascade::ActiveSpeedAlgorithm() const
    {
        return ActiveSpeedAlgorithmImpl();
    }

    void SpeedCascade::SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator)
    {
        SetOnlineMechanicalEstimatorImpl(estimator);
    }

    void SpeedCascade::SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator)
    {
        SetOnlineElectricalEstimatorImpl(estimator);
    }

    void SpeedCascade::Enable()
    {
        EnableSpeedLoop();
        SetPoint(lastSpeedSetPoint);
    }

    void SpeedCascade::Disable()
    {
        DisableSpeedLoop();
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    OPTIMIZE_FOR_SPEED
    void SpeedCascade::LowPriorityHandler()
    {
        auto mechanicalSpeed = MeasureMechanicalSpeed();
        RunSpeedLoop(mechanicalSpeed);

        UpdateOnlineMechanicalEstimator(mechanicalSpeed);
        UpdateOnlineElectricalEstimator();
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif

    MotionObservation SpeedCascade::ObserveMotion() const
    {
        return ObserveSpeedLoopMotion();
    }

    hal::Hertz SpeedCascade::OuterLoopFrequency() const
    {
        return outerLoopFrequency;
    }

    void SpeedCascade::EnableSpeedCommand()
    {
        Enable();
    }

    void SpeedCascade::DisableSpeedCommand()
    {
        Disable();
    }

    void SpeedCascade::CommandSpeed(RadiansPerSecond speed)
    {
        SetPoint(speed);
    }

    hal::Hertz SpeedCascade::SpeedCommandFrequency() const
    {
        return outerLoopFrequency;
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles SpeedCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif
}
