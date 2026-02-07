#include "source/foc/implementations/FocTorqueImpl.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "source/foc/instantiations/TrigonometricImpl.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace
{
    constexpr float invSqrt3 = 0.577350269189625f;
}

namespace foc
{
    OPTIMIZE_FOR_SPEED
    void FocTorqueImpl::Reset()
    {
        dPid.Disable();
        qPid.Disable();

        dPid.Enable();
        qPid.Enable();
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
    void FocTorqueImpl::SetPolePairs(std::size_t polePairs)
    {
        this->polePairs = static_cast<float>(polePairs);
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles FocTorqueImpl::Calculate(const PhaseCurrents& currentPhases, Radians& mechanicalAngle)
    {
        const float ia = currentPhases.a.Value();
        const float ib = currentPhases.b.Value();
        const float ic = currentPhases.c.Value();

        const auto electricalAngle = mechanicalAngle * polePairs;

        auto cosTheta = FastTrigonometry::Cosine(electricalAngle.Value());
        auto sinTheta = FastTrigonometry::Sine(electricalAngle.Value());

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        auto output = spaceVectorModulator.Generate(park.Inverse(RotatingFrame{ dPid.Process(idAndIq.d), qPid.Process(idAndIq.q) }, cosTheta, sinTheta));

        return PhasePwmDutyCycles{ hal::Percent(static_cast<uint8_t>(output.a * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.b * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.c * 100.0f + 0.5f)) };
    }
}
