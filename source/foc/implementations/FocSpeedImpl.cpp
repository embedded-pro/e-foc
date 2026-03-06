#include "source/foc/implementations/FocSpeedImpl.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    OPTIMIZE_FOR_SPEED
    FocSpeedImpl::FocSpeedImpl(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : FocWithSpeedLoop(maxCurrent, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency)
    {
        lowPriorityInterrupt.Register([this]()
            {
                LowPriorityHandler();
            });
    }

    void FocSpeedImpl::SetPolePairs(std::size_t pole)
    {
        SetPolePairsImpl(pole);
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::SetPoint(RadiansPerSecond point)
    {
        lastSpeedSetPoint = point;
        speedPid.SetPoint(point.Value());
        dPid.SetPoint(0.0f);
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings)
    {
        SetCurrentTuningsImpl(Vdc, torqueTunings);
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning)
    {
        SetSpeedTuningsImpl(speedTuning);
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::Enable()
    {
        EnableSpeedLoop();
        SetPoint(lastSpeedSetPoint);
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::Disable()
    {
        DisableSpeedLoop();
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::LowPriorityHandler()
    {
        auto mechanicalSpeed = detail::PositionWithWrapAround(currentMechanicalAngle - previousSpeedPosition) / speedDt;
        previousSpeedPosition = currentMechanicalAngle;
        lastSpeedPidOutput = speedPid.Process(mechanicalSpeed);
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocSpeedImpl::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        return CalculateInnerLoop(currentPhases, position);
    }
}
