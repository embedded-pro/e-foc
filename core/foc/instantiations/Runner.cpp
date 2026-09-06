#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/foc/instantiations/Runner.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

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
        inverter.Start();
        enabled = true;
    }

    void Runner::Disable()
    {
        enabled = false;
        inverter.Stop();
        // Releasing the slot is what keeps a stopped drive stopped: a callback left pointing here
        // writes duty cycles on the next conversion, and ThreePhasePwmOutput re-arms the peripheral
        // Stop() just disabled. Safe against the ADC interrupt because the only other context that
        // reaches this - the PWM fault interrupt - shares its priority and so cannot preempt it.
        ReleasePhaseCurrents();
        foc.Disable();
    }

    void Runner::RegisterPhaseCurrentsObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer)
    {
        phaseCurrentsObserver = observer;
        observerRegistered = true;
    }

    void Runner::UnregisterPhaseCurrentsObserver()
    {
        observerRegistered = false;
        phaseCurrentsObserver = nullptr;
    }

    void Runner::RegisterPhaseCurrents()
    {
        inverter.PhaseCurrentsReady(inverter.BaseFrequency(), [this](auto currentPhases)
            {
                OnPhaseCurrents(currentPhases);
            });
    }

    void Runner::ReleasePhaseCurrents()
    {
        inverter.PhaseCurrentsReady(inverter.BaseFrequency(), [](auto) {});
    }

    OPTIMIZE_FOR_SPEED
    void Runner::OnPhaseCurrents(const PhaseCurrents& currentPhases)
    {
        if (!enabled)
            return;

        auto position = encoder.Read();
        inverter.ThreePhasePwmOutput(foc.Calculate(currentPhases, position));

        if (observerRegistered)
            phaseCurrentsObserver(currentPhases);
    }
}
