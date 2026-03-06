#include "source/foc/implementations/FocPositionImpl.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    OPTIMIZE_FOR_SPEED
    FocPositionImpl::FocPositionImpl(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : FocWithSpeedLoop(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency)
        , positionPid{ { 0.0f, 0.0f, 0.0f }, { -1000.0f, 1000.0f } }
    {
        positionPid.Enable();

        lowPriorityInterrupt.Register([this]()
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
        dPid.SetPoint(0.0f);
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

        positionPid.SetTunings({ kp, ki * speedDt, kd / speedDt });
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
        auto mechanicalSpeed = detail::PositionWithWrapAround(currentMechanicalAngle - previousSpeedPosition) / speedDt;
        previousSpeedPosition = currentMechanicalAngle;

        auto speedReference = positionPid.Process(currentMechanicalAngle);
        speedPid.SetPoint(speedReference);
        lastSpeedPidOutput = speedPid.Process(mechanicalSpeed);
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocPositionImpl::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }
}
