#include "source/foc/implementations/FocWithSpeedPidImpl.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <numbers>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float two_pi = 2.0f * pi;

    OPTIMIZE_FOR_SPEED
    float PositionWithWrapAround(float position)
    {
        if (position > pi)
            position -= two_pi;
        else if (position < -pi)
            position += two_pi;

        return position;
    }
}

namespace foc
{
    OPTIMIZE_FOR_SPEED
    FocWithSpeedPidImpl::FocWithSpeedPidImpl(LowPriorityInterrupt& lowPriorityInterrupt, foc::Ampere maxCurrent, hal::Hertz pwmFrequency, const NyquistFactor& nyquistFactor)
        : lowPriorityInterrupt{ lowPriorityInterrupt }
        , speedPid{ { 0.0f, 0.0f, 0.0f }, { -maxCurrent.Value(), maxCurrent.Value() } }
        , nyquistFactor{ static_cast<uint8_t>(nyquistFactor.Value()) }
        , speedLoopPeriod{ this->nyquistFactor / static_cast<float>(pwmFrequency.Value()) }
    {
        really_assert(maxCurrent.Value() > 0);

        speedPid.Enable();

        lowPriorityInterrupt.Register([this]()
            {
                focTorqueImpl.SetPoint(std::make_pair(foc::Ampere{ 0.0f }, foc::Ampere{ targetSpeed.load() }));
            });
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedPidImpl::SetPoint(RadiansPerSecond point)
    {
        speedPid.SetPoint(point.Value());
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedPidImpl::SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings)
    {
        focTorqueImpl.SetCurrentTunings(Vdc, torqueTunings);
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedPidImpl::SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning)
    {
        const float kp = speedTuning.kp;
        const float ki = speedTuning.ki;
        const float kd = speedTuning.kd;

        speedPid.SetTunings({ kp, ki * speedLoopPeriod, kd / speedLoopPeriod });
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedPidImpl::Reset()
    {
        speedPid.Disable();
        speedPid.Enable();
        focTorqueImpl.Reset();

        previousPosition = 0.0f;
        speedLoopCounter = 0;
    }

    OPTIMIZE_FOR_SPEED
    float FocWithSpeedPidImpl::CalculateFilteredSpeed(float mechanicalPosition)
    {
        auto mechanicalSpeed = PositionWithWrapAround(mechanicalPosition - previousPosition) / speedLoopPeriod;

        previousPosition = mechanicalPosition;

        return mechanicalSpeed;
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedPidImpl::ExecuteSpeedControlLoop(Radians& position)
    {
        targetSpeed.store(speedPid.Process(CalculateFilteredSpeed(position.Value())));
        lowPriorityInterrupt.Trigger();
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocWithSpeedPidImpl::Calculate(const PhaseCurrents& currentPhases, Radians& mechanicalAngle)
    {
        speedLoopCounter++;
        if (speedLoopCounter >= nyquistFactor)
        {
            speedLoopCounter = 0;
            ExecuteSpeedControlLoop(mechanicalAngle);
        }

        return focTorqueImpl.Calculate(currentPhases, mechanicalAngle);
    }
}
