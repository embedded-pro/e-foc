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

    void PositionCascade::SetPolePairs(std::size_t pole)
    {
        SetPolePairsImpl(pole);
    }

    OPTIMIZE_FOR_SPEED
    void PositionCascade::SetPoint(Radians point)
    {
        lastPositionSetPoint = point;
        positionPid.SetPoint(point.Value());
        DPid().SetPoint(0.0f);
    }

    OPTIMIZE_FOR_SPEED
    void PositionCascade::SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings)
    {
        SetCurrentTuningsImpl(Vdc, torqueTunings);
    }

    OPTIMIZE_FOR_SPEED
    void PositionCascade::SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning)
    {
        SetSpeedTuningsImpl(speedTuning);
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
    void PositionCascade::SetPositionTunings(const PositionTunings& positionTuning)
    {
        const float kp = positionTuning.kp;
        const float ki = positionTuning.ki;
        const float kd = positionTuning.kd;

        positionPid.SetTunings({ kp, ki * SpeedDt(), kd / SpeedDt() });
    }

    OPTIMIZE_FOR_SPEED
    void PositionCascade::Enable()
    {
        positionPid.Reset();
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
        auto mechanicalSpeed = detail::PositionWithWrapAround(CurrentMechanicalAngle() - PreviousSpeedPosition()) / SpeedDt();
        PreviousSpeedPosition() = CurrentMechanicalAngle();

        auto speedReference = positionPid.Process(CurrentMechanicalAngle());
        SpeedPid().SetPoint(speedReference);
        LastSpeedPidOutput() = SpeedPid().Process(mechanicalSpeed);

        UpdateOnlineMechanicalEstimator(mechanicalSpeed);
        UpdateOnlineElectricalEstimator(mechanicalSpeed * PolePairs());
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles PositionCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }
}
