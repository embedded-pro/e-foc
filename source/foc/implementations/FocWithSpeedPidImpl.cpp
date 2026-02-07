#include "source/foc/implementations/FocWithSpeedPidImpl.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <numbers>
#include <utility>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float two_pi = 2.0f * pi;

    OPTIMIZE_FOR_SPEED
    foc::Radians PositionWithWrapAround(foc::Radians position)
    {
        if (position.Value() > pi)
            position -= foc::Radians{ two_pi };
        else if (position.Value() < -pi)
            position += foc::Radians{ two_pi };

        return position;
    }
}

namespace foc
{
    OPTIMIZE_FOR_SPEED
    FocWithSpeedPidImpl::FocWithSpeedPidImpl(LowPriorityInterrupt& lowPriorityInterrupt, foc::Ampere maxCurrent, hal::Hertz pwmFrequency, const NyquistFactor& nyquistFactor)
        : lowPriorityInterrupt{ lowPriorityInterrupt }
        , speedPid{ { 0.0f, 0.0f, 0.0f }, { -maxCurrent.Value(), maxCurrent.Value() } }
        , speedLoopCounter{ static_cast<uint8_t>(nyquistFactor.Value()) }
        , nyquistFactor{ static_cast<uint8_t>(nyquistFactor.Value()) }
        , speedLoopPeriod{ this->nyquistFactor / static_cast<float>(pwmFrequency.Value()) }
    {
        really_assert(maxCurrent.Value() > 0);

        speedPid.Enable();

        lowPriorityInterrupt.Register([this]()
            {
                focTorqueImpl.SetPoint(std::make_pair(foc::Ampere{ 0.0f }, foc::Ampere{ targetTorque.load().Value() }));
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

        previousPosition = Radians{ 0.0f };
        speedLoopCounter = nyquistFactor;
    }

    OPTIMIZE_FOR_SPEED
    RadiansPerSecond FocWithSpeedPidImpl::FilteredSpeed(Radians mechanicalPosition)
    {
        auto mechanicalSpeed = RadiansPerSecond{ PositionWithWrapAround(mechanicalPosition - previousPosition).Value() / speedLoopPeriod };

        previousPosition = mechanicalPosition;

        return mechanicalSpeed;
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedPidImpl::ExecuteSpeedControlLoop(Radians&, foc::PhaseCurrents, RadiansPerSecond, Radians, foc::NewtonMeter)
    {
    }

    FocTorqueImpl& FocWithSpeedPidImpl::FocTorque()
    {
        return focTorqueImpl;
    }

    OPTIMIZE_FOR_SPEED
    void FocWithSpeedPidImpl::ExecuteSpeedLoop(const PhaseCurrents& currentPhases, Radians& mechanicalAngle)
    {
        auto speed = FilteredSpeed(mechanicalAngle);
        auto torque = foc::NewtonMeter{ speedPid.Process(speed.Value()) };
        targetTorque.store(torque);
        lowPriorityInterrupt.Trigger();
        ExecuteSpeedControlLoop(mechanicalAngle, currentPhases, speed, mechanicalAngle, torque);
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocWithSpeedPidImpl::Calculate(const PhaseCurrents& currentPhases, Radians& mechanicalAngle)
    {
        if (std::exchange(speedLoopCounter, speedLoopCounter > 1 ? speedLoopCounter - 1 : nyquistFactor) == 1)
            ExecuteSpeedLoop(currentPhases, mechanicalAngle);

        return focTorqueImpl.Calculate(currentPhases, mechanicalAngle);
    }
}
