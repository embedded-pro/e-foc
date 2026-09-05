#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/instantiations/Runner.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <cmath>

namespace foc
{
    Runner::Runner(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, FocBase& foc)
        : inverter{ inverter }
        , encoder{ encoder }
        , foc{ foc }
    {
        RegisterPhaseCurrents();
    }

    Runner::~Runner()
    {
        Disable();
    }

    void Runner::Enable()
    {
        RegisterPhaseCurrents();
        foc.Enable();
        implausibleSamples = 0;
        inverter.Start();
        enabled = true;
    }

    void Runner::RegisterOnImplausibleCurrents(const infra::Function<void()>& onImplausible)
    {
        onImplausibleCurrents = onImplausible;
    }

    void Runner::Disable()
    {
        enabled = false;
        inverter.Stop();
        foc.Disable();
    }

    void Runner::RegisterPhaseCurrents()
    {
        inverter.PhaseCurrentsReady(inverter.BaseFrequency(), [this](auto currentPhases)
            {
                OnPhaseCurrents(currentPhases);
            });
    }

    OPTIMIZE_FOR_SPEED
    void Runner::OnPhaseCurrents(const PhaseCurrents& currentPhases)
    {
        if (!enabled)
            return;

        if (!CurrentsArePlausible(currentPhases))
        {
            Disable();

            if (onImplausibleCurrents != nullptr)
                onImplausibleCurrents();

            return;
        }

        auto position = encoder.Read();
        inverter.ThreePhasePwmOutput(foc.Calculate(currentPhases, position));
    }

    OPTIMIZE_FOR_SPEED
    bool Runner::CurrentsArePlausible(const PhaseCurrents& currentPhases)
    {
        const auto a = currentPhases.a.Value();
        const auto b = currentPhases.b.Value();
        const auto c = currentPhases.c.Value();

        if (!IsFiniteValue(a) || !IsFiniteValue(b) || !IsFiniteValue(c))
            return false;

        const auto magnitude = std::fabs(a) + std::fabs(b) + std::fabs(c);
        const auto tolerance = residualFloor + residualFraction * magnitude;

        if (std::fabs(a + b + c) <= tolerance)
        {
            implausibleSamples = 0;
            return true;
        }

        if (implausibleSamples < implausibleSampleLimit)
        {
            ++implausibleSamples;
            return true;
        }

        return false;
    }
}
