#include "core/foc/cascade/SpeedCascade.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    OPTIMIZE_FOR_SPEED
    SpeedCascade::SpeedCascade(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : CascadeWithSpeedLoop(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency)
        , outerLoopFrequency(lowPriorityFrequency)
    {
        GetLowPriorityInterrupt().Register([this]()
            {
                LowPriorityHandler();
            });
    }

    void SpeedCascade::SetPolePairs(std::size_t pole)
    {
        SetPolePairsImpl(pole);
    }

    OPTIMIZE_FOR_SPEED
    void SpeedCascade::SetPoint(RadiansPerSecond point)
    {
        lastSpeedSetPoint = point;
        SpeedPid().SetPoint(point.Value());
        DPid().SetPoint(0.0f);
    }

    OPTIMIZE_FOR_SPEED
    void SpeedCascade::SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings)
    {
        SetCurrentTuningsImpl(Vdc, torqueTunings);
    }

    OPTIMIZE_FOR_SPEED
    void SpeedCascade::SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning)
    {
        SetSpeedTuningsImpl(speedTuning);
    }

    void SpeedCascade::SetOnlineMechanicalEstimator(OnlineMechanicalEstimator& estimator)
    {
        SetOnlineMechanicalEstimatorImpl(estimator);
    }

    void SpeedCascade::SetOnlineElectricalEstimator(OnlineElectricalEstimator& estimator)
    {
        SetOnlineElectricalEstimatorImpl(estimator);
    }

    OPTIMIZE_FOR_SPEED
    void SpeedCascade::Enable()
    {
        EnableSpeedLoop();
        SetPoint(lastSpeedSetPoint);
    }

    OPTIMIZE_FOR_SPEED
    void SpeedCascade::Disable()
    {
        DisableSpeedLoop();
    }

    OPTIMIZE_FOR_SPEED
    void SpeedCascade::LowPriorityHandler()
    {
        auto mechanicalSpeed = detail::PositionWithWrapAround(CurrentMechanicalAngle() - PreviousSpeedPosition()) / SpeedDt();
        PreviousSpeedPosition() = CurrentMechanicalAngle();
        LastSpeedPidOutput() = SpeedPid().Process(mechanicalSpeed);

        UpdateOnlineMechanicalEstimator(mechanicalSpeed);
        UpdateOnlineElectricalEstimator(mechanicalSpeed * PolePairs());
    }

    hal::Hertz SpeedCascade::OuterLoopFrequency() const
    {
        return outerLoopFrequency;
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles SpeedCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }
}
