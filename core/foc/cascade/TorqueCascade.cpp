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
        speedFilter.Reset();
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

    bool TorqueCascade::Configure(const MotorModelParameters& parameters)
    {
        polePairs = static_cast<float>(parameters.polePairs);
        const bool ok = currentLoop.Configure(parameters);

        const auto samplingFrequency = static_cast<float>(parameters.samplingFrequency.Value());
        electricalSpeedScale = polePairs * samplingFrequency;
        speedFilter.SetAlpha(1.0f - math::Exp(-1.0f / (speedFilterTimeConstant * samplingFrequency)));
        return ok;
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

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("O3", "fast-math")
#endif
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
        return speedFilter.Filter(sampled);
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC pop_options
#endif
}
