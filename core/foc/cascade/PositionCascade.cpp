#include "core/foc/cascade/PositionCascade.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    OPTIMIZE_FOR_SPEED
    PositionCascade::PositionCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : CascadeWithSpeedLoop(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency)
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
        ConfigureMechanicsImpl(parameters);
    }

    OPTIMIZE_FOR_SPEED
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

    void PositionCascade::SetPositionTunings(const PositionLoopTunings& tunings)
    {
        positionGain = tunings.bandwidth;
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

    void PositionCascade::SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator)
    {
        SetOnlineMechanicalEstimatorImpl(estimator);
    }

    void PositionCascade::SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator)
    {
        SetOnlineElectricalEstimatorImpl(estimator);
    }

    OPTIMIZE_FOR_SPEED
    void PositionCascade::Enable()
    {
        EnableSpeedLoop();
        SetPoint(lastPositionSetPoint);
    }

    OPTIMIZE_FOR_SPEED
    void PositionCascade::Disable()
    {
        DisableSpeedLoop();
    }

    OPTIMIZE_FOR_SPEED
    void PositionCascade::LowPriorityHandler()
    {
        auto mechanicalSpeed = MeasureMechanicalSpeed();

        auto positionError = detail::PositionWithWrapAround(lastPositionSetPoint.Value() - CurrentMechanicalAngle());
        SetSpeedReference(RadiansPerSecond{ positionError * positionGain });

        RunSpeedLoop(mechanicalSpeed);

        UpdateOnlineMechanicalEstimator(mechanicalSpeed);
        UpdateOnlineElectricalEstimator(mechanicalSpeed * PolePairs());
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles PositionCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }
}
