#include "source/foc/instantiations/FocImpl.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "source/foc/instantiations/TrigonometricImpl.hpp"
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
    void FocTorqueImpl::Enable()
    {
        dPid.Enable();
        qPid.Enable();
    }

    OPTIMIZE_FOR_SPEED
    void FocTorqueImpl::Disable()
    {
        dPid.Disable();
        qPid.Disable();
    }

    void FocTorqueImpl::SetPolePairs(std::size_t polePairs)
    {
        this->polePairs = static_cast<float>(polePairs);
    }

    OPTIMIZE_FOR_SPEED
    void FocTorqueImpl::SetPoint(IdAndIqPoint setPoint)
    {
        dPid.SetPoint(setPoint.first.Value());
        qPid.SetPoint(setPoint.second.Value());
    }

    OPTIMIZE_FOR_SPEED
    void FocTorqueImpl::SetCurrentTunings(Volts Vdc, const IdAndIqTunings& tunings)
    {
        auto scale = 1.0f / (invSqrt3 * Vdc.Value());

        const float d_kp = tunings.first.kp;
        const float d_ki = tunings.first.ki;
        const float d_kd = tunings.first.kd;
        const float q_kp = tunings.second.kp;
        const float q_ki = tunings.second.ki;
        const float q_kd = tunings.second.kd;

        dPid.SetTunings({ d_kp * scale, d_ki * scale, d_kd * scale });
        qPid.SetTunings({ q_kp * scale, q_ki * scale, q_kd * scale });
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocTorqueImpl::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        const float ia = currentPhases.a.Value();
        const float ib = currentPhases.b.Value();
        const float ic = currentPhases.c.Value();

        auto mechanicalAngle = position.Value();
        auto electricalAngle = mechanicalAngle * polePairs;

        auto cosTheta = FastTrigonometry::Cosine(electricalAngle);
        auto sinTheta = FastTrigonometry::Sine(electricalAngle);

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        auto output = spaceVectorModulator.Generate(park.Inverse(RotatingFrame{ dPid.Process(idAndIq.d), qPid.Process(idAndIq.q) }, cosTheta, sinTheta));

        return PhasePwmDutyCycles{ hal::Percent(static_cast<uint8_t>(output.a * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.b * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.c * 100.0f + 0.5f)) };
    }

    OPTIMIZE_FOR_SPEED
    FocSpeedImpl::FocSpeedImpl(foc::Ampere maxCurrent, std::chrono::system_clock::duration timeStep)
        : speedPid{ { 0.0f, 0.0f, 0.0f }, { -maxCurrent.Value(), maxCurrent.Value() } }
        , dt{ std::chrono::duration_cast<std::chrono::duration<float>>(timeStep).count() }
    {
        really_assert(maxCurrent.Value() > 0);

        speedPid.Enable();
        dPid.Enable();
        qPid.Enable();
    }

    void FocSpeedImpl::SetPolePairs(std::size_t pole)
    {
        polePairs = static_cast<float>(pole);
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::SetPoint(RadiansPerSecond point)
    {
        speedPid.SetPoint(point.Value());
        dPid.SetPoint(0.0f);
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::SetCurrentTunings(Volts Vdc, const IdAndIqTunings& torqueTunings)
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
    void FocSpeedImpl::SetSpeedTunings(Volts Vdc, const SpeedTunings& speedTuning)
    {
        const float kp = speedTuning.kp;
        const float ki = speedTuning.ki;
        const float kd = speedTuning.kd;

        speedPid.SetTunings({ kp, ki * dt, kd / dt });
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::Enable()
    {
        speedPid.Enable();
        dPid.Enable();
        qPid.Enable();

        previousPosition = 0.0f;
    }

    OPTIMIZE_FOR_SPEED
    void FocSpeedImpl::Disable()
    {
        speedPid.Disable();
        dPid.Disable();
        qPid.Disable();
    }

    OPTIMIZE_FOR_SPEED
    float FocSpeedImpl::CalculateFilteredSpeed(float mechanicalPosition)
    {
        auto mechanicalSpeed = PositionWithWrapAround(mechanicalPosition - previousPosition) / dt;

        previousPosition = mechanicalPosition;

        return mechanicalSpeed;
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocSpeedImpl::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        const float ia = currentPhases.a.Value();
        const float ib = currentPhases.b.Value();
        const float ic = currentPhases.c.Value();

        auto mechanicalAngle = position.Value();
        auto electricalAngle = mechanicalAngle * polePairs;

        auto cosTheta = FastTrigonometry::Cosine(electricalAngle);
        auto sinTheta = FastTrigonometry::Sine(electricalAngle);

        qPid.SetPoint(speedPid.Process(CalculateFilteredSpeed(mechanicalAngle)));

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        auto output = spaceVectorModulator.Generate(park.Inverse(RotatingFrame{ dPid.Process(idAndIq.d), qPid.Process(idAndIq.q) }, cosTheta, sinTheta));

        return PhasePwmDutyCycles{ hal::Percent(static_cast<uint8_t>(output.a * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.b * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.c * 100.0f + 0.5f)) };
    }
}
