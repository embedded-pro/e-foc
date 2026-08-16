#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

namespace foc
{
    OPTIMIZE_FOR_SPEED
    void TorqueCascade::Enable()
    {
        currentLoop.Reset();
        SetPoint(lastSetPoint);
    }

    OPTIMIZE_FOR_SPEED
    void TorqueCascade::Disable()
    {
    }

    void TorqueCascade::Configure(const MotorModelParameters& parameters)
    {
        polePairs = static_cast<float>(parameters.polePairs);
        currentLoop.Configure(parameters);
    }

    OPTIMIZE_FOR_SPEED
    void TorqueCascade::SetPoint(IdAndIqPoint setPoint)
    {
        lastSetPoint = setPoint;
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
        auto voltage = currentLoop.Compute(CurrentControlContext{ idAndIq, RotatingFrame{ lastSetPoint.first.Value(), lastSetPoint.second.Value() }, 0.0f });
        auto output = spaceVectorModulator.Generate(park.Inverse(voltage, cosTheta, sinTheta));

        return PhasePwmDutyCycles{ hal::Percent(static_cast<uint8_t>(output.a * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.b * 100.0f + 0.5f)),
            hal::Percent(static_cast<uint8_t>(output.c * 100.0f + 0.5f)) };
    }
}
