#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/DutyConversion.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "numerical/math/Math.hpp"

namespace foc
{
    namespace
    {
        constexpr float speedFilterTimeConstant = 0.001f;
    }

    TorqueCascade::TorqueCascade(foc::Ampere maxCurrent)
        : maxCurrent{ maxCurrent }
    {
        really_assert(maxCurrent.Value() > 0);
    }

    void TorqueCascade::Enable()
    {
        currentLoop.Reset();
        enabled = true;
        electricalSpeed = 0.0f;
        previousAngleValid = false;
        SetPoint(lastSetPoint);
    }

    void TorqueCascade::Disable()
    {
        enabled = false;
    }

    SelectResult TorqueCascade::SelectCurrentAlgorithm(CurrentAlgorithm algorithm)
    {
        if (enabled)
            return SelectResult::busy;

        return currentLoop.Select(algorithm);
    }

    CurrentAlgorithm TorqueCascade::ActiveCurrentAlgorithm() const
    {
        return currentLoop.Active();
    }

    void TorqueCascade::Configure(const MotorModelParameters& parameters)
    {
        polePairs = static_cast<float>(parameters.polePairs);
        currentLoop.Configure(parameters);

        const auto samplingFrequency = static_cast<float>(parameters.samplingFrequency.Value());
        if (samplingFrequency <= 0.0f)
            return;

        electricalSpeedScale = polePairs * samplingFrequency;
        // Smooth to the same 1 ms the speed cascade measures over, so the feedforward tracks the
        // rotor without passing encoder quantisation through to Vd and Vq.
        speedFilterAlpha = 1.0f - math::Exp(-1.0f / (speedFilterTimeConstant * samplingFrequency));
    }

    void TorqueCascade::SetPoint(IdAndIqPoint setPoint)
    {
        lastSetPoint = LimitToCurrentEnvelope(setPoint);
    }

    IdAndIqPoint TorqueCascade::LimitToCurrentEnvelope(IdAndIqPoint setPoint) const
    {
        const auto d = setPoint.first.Value();
        const auto q = setPoint.second.Value();
        const auto squaredMagnitude = d * d + q * q;
        const auto limit = maxCurrent.Value();

        if (squaredMagnitude <= limit * limit)
            return setPoint;

        const auto scale = limit / math::Sqrt(squaredMagnitude);

        return { Ampere{ d * scale }, Ampere{ q * scale } };
    }

    void TorqueCascade::SetCurrentTunings(const CurrentLoopTunings& tunings)
    {
        currentLoop.SetTunings(tunings);
    }

    CurrentControllerSelector& TorqueCascade::CurrentLoop()
    {
        return currentLoop;
    }

    OPTIMIZE_FOR_SPEED
    PhasePwmDutyCycles TorqueCascade::Calculate(const PhaseCurrents& currentPhases, Radians& position)
    {
        const float ia = currentPhases.a.Value();
        const float ib = currentPhases.b.Value();
        const float ic = currentPhases.c.Value();

        auto mechanicalAngle = position.Value();
        auto electricalAngle = mechanicalAngle * polePairs;

        auto cosTheta = FastTrigonometry::Cosine(electricalAngle);
        auto sinTheta = FastTrigonometry::Sine(electricalAngle);

        auto idAndIq = park.Forward(clarke.Forward(ThreePhase{ ia, ib, ic }), cosTheta, sinTheta);
        auto voltage = currentLoop.Compute(CurrentControlContext{ idAndIq, RotatingFrame{ lastSetPoint.first.Value(), lastSetPoint.second.Value() }, MeasureElectricalSpeed(mechanicalAngle) });
        auto output = spaceVectorModulator.Generate(park.Inverse(voltage, cosTheta, sinTheta));

        return ToDutyCycles(output);
    }

    OPTIMIZE_FOR_SPEED
    float TorqueCascade::MeasureElectricalSpeed(float mechanicalAngle)
    {
        if (!previousAngleValid)
        {
            previousMechanicalAngle = mechanicalAngle;
            previousAngleValid = true;
            return 0.0f;
        }

        const auto sampled = detail::PositionWithWrapAround(mechanicalAngle - previousMechanicalAngle) * electricalSpeedScale;
        previousMechanicalAngle = mechanicalAngle;
        electricalSpeed += speedFilterAlpha * (sampled - electricalSpeed);

        return electricalSpeed;
    }
}
