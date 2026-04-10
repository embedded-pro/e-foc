#include "core/foc/implementations/FocPositionImpl.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    OPTIMIZE_FOR_SPEED
    FocPositionImpl::FocPositionImpl(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : FocWithSpeedLoop(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency)
    {
        positionPid.Enable();

        GetLowPriorityInterrupt().Register([this]()
            {
                LowPriorityHandler();
            });
    }

    void FocPositionImpl::SetPolePairs(std::size_t pole)
    {
        SetPolePairsImpl(pole);
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::SetPoint(Radians point)
    {
        lastPositionSetPoint = point;
        positionPid.SetPoint(point.Value());
        DPid().SetPoint(0.0f);
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings)
    {
        SetCurrentTuningsImpl(Vdc, torqueTunings);
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning)
    {
        SetSpeedTuningsImpl(speedTuning);
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::SetPositionTunings(const PositionTunings& positionTuning)
    {
        const float kp = positionTuning.kp;
        const float ki = positionTuning.ki;
        const float kd = positionTuning.kd;

        positionPid.SetTunings({ kp, ki * SpeedDt(), kd / SpeedDt() });
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::Enable()
    {
        positionPid.Enable();
        EnableSpeedLoop();
        SetPoint(lastPositionSetPoint);
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::Disable()
    {
        positionPid.Disable();
        DisableSpeedLoop();
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::LowPriorityHandler()
    {
        auto mechanicalSpeed = detail::PositionWithWrapAround(CurrentMechanicalAngle() - PreviousSpeedPosition()) / SpeedDt();
        PreviousSpeedPosition() = CurrentMechanicalAngle();

        auto speedReference = positionPid.Process(CurrentMechanicalAngle());
        SpeedPid().SetPoint(speedReference);
        LastSpeedPidOutput() = SpeedPid().Process(mechanicalSpeed);
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocPositionImpl::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }
}
