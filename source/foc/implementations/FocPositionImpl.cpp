#include "source/foc/implementations/FocPositionImpl.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "source/foc/implementations/TrigonometricImpl.hpp"
#include <numbers>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    constexpr float invSqrt3 = 0.577350269189625f;
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
    FocPositionImpl::FocPositionImpl(foc::Ampere maxCurrent, hal::Hertz baseFrequency, LowPriorityInterrupt& lowPriorityInterrupt, hal::Hertz lowPriorityFrequency)
        : positionPid{ { 0.0f, 0.0f, 0.0f }, { -1000.0f, 1000.0f } }
        , speedPid{ { 0.0f, 0.0f, 0.0f }, { -maxCurrent.Value(), maxCurrent.Value() } }
        , lowPriorityInterrupt(lowPriorityInterrupt)
        , dt{ 1.0f / static_cast<float>(baseFrequency.Value()) }
        , speedDt{ 1.0f / static_cast<float>(lowPriorityFrequency.Value()) }
        , prescaler{ baseFrequency.Value() / lowPriorityFrequency.Value() }
    {
        really_assert(maxCurrent.Value() > 0);
        really_assert(lowPriorityFrequency.Value() > 0);
        really_assert(lowPriorityFrequency.Value() <= baseFrequency.Value());
        really_assert(baseFrequency.Value() % lowPriorityFrequency.Value() == 0);

        positionPid.Enable();
        speedPid.Enable();
        dPid.Enable();
        qPid.Enable();

        lowPriorityInterrupt.Register([this]()
            {
                LowPriorityHandler();
            });
    }

    void FocPositionImpl::SetPolePairs(std::size_t pole)
    {
        polePairs = static_cast<float>(pole);
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
        auto scale = 1.0f / (invSqrt3 * Vdc.Value());
        auto scale_dt = scale * dt;
        auto scale_inv_dt = scale / dt;

        const float d_kp = torqueTunings.first.kp;
        const float d_ki = torqueTunings.first.ki;
        const float d_kd = torqueTunings.first.kd;
        const float q_kp = torqueTunings.second.kp;
        const float q_ki = torqueTunings.second.ki;
        const float q_kd = torqueTunings.second.kd;

        dPid.SetTunings({ d_kp * scale, d_ki * scale_dt, d_kd * scale_inv_dt });
        qPid.SetTunings({ q_kp * scale, q_ki * scale_dt, q_kd * scale_inv_dt });
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning)
    {
        const float kp = speedTuning.kp;
        const float ki = speedTuning.ki;
        const float kd = speedTuning.kd;

        speedPid.SetTunings({ kp, ki * speedDt, kd / speedDt });
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
        speedPid.Enable();
        dPid.Enable();
        qPid.Enable();

        currentMechanicalAngle = 0.0f;
        previousSpeedPosition = 0.0f;
        lastSpeedPidOutput = 0.0f;
        triggerCounter = 0;
        SetPoint(lastPositionSetPoint);
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::Disable()
    {
        positionPid.Disable();
        speedPid.Disable();
        dPid.Disable();
        qPid.Disable();
    }

    OPTIMIZE_FOR_SPEED
    void FocPositionImpl::LowPriorityHandler()
    {
        auto mechanicalSpeed = PositionWithWrapAround(currentMechanicalAngle - previousSpeedPosition) / speedDt;
        previousSpeedPosition = currentMechanicalAngle;

        auto speedReference = positionPid.Process(currentMechanicalAngle);
        speedPid.SetPoint(speedReference);
        lastSpeedPidOutput = speedPid.Process(mechanicalSpeed);
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocPositionImpl::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        const float ia = currentPhases.a.Value();
        const float ib = currentPhases.b.Value();
        const float ic = currentPhases.c.Value();

        auto mechanicalAngle = position.Value();
        auto electricalAngle = mechanicalAngle * polePairs;
        currentMechanicalAngle = mechanicalAngle;

        auto cosTheta = FastTrigonometry::Cosine(electricalAngle);
        auto sinTheta = FastTrigonometry::Sine(electricalAngle);

        qPid.SetPoint(lastSpeedPidOutput);

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        auto output = spaceVectorModulator.Generate(park.Inverse(RotatingFrame{ dPid.Process(idAndIq.d), qPid.Process(idAndIq.q) }, cosTheta, sinTheta));

        ++triggerCounter;
        if (triggerCounter >= prescaler)
        {
            triggerCounter = 0;
            lowPriorityInterrupt.Trigger();
        }

        return PhasePwmDutyCycles{ hal::Percent(static_cast<uint8_t>(output.a * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.b * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.c * 100.0f + 0.5f)) };
    }
}
