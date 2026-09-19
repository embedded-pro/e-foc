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
        const auto sequence = stopSequence;

        RegisterPhaseCurrents();

        if (StoppedSince(sequence))
        {
            Disable();
            return;
        }

        foc.Enable();

        if (StoppedSince(sequence))
        {
            Disable();
            return;
        }

        inverter.Start();

        if (StoppedSince(sequence))
        {
            Disable();
            return;
        }

        enabled = true;

        // A stop from the faulting context between two of these steps would otherwise be undone by the
        // steps that follow it, re-arming the bridge on faulted hardware. A stop landing after this last
        // check needs no undoing: it runs after every write this sequence makes.
        if (StoppedSince(sequence))
            Disable();
    }

    void Runner::Disable()
    {
        stopSequence = static_cast<uint8_t>(stopSequence + 1u);
        enabled = false;
        inverter.Stop();
        ReleasePhaseCurrents();
        foc.Disable();
    }

    bool Runner::StoppedSince(uint8_t sequence) const
    {
        return stopSequence != sequence;
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
