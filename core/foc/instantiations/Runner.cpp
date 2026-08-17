#include "core/foc/instantiations/Runner.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

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
        // Calibration services take over the single-slot current callback, so it must be reclaimed here.
        RegisterPhaseCurrents();
        foc.Enable();
        enabled = true;
        inverter.Start();
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
        // A conversion already in flight when Stop() ran must not write duty cycles: on TI that
        // re-enables the generator and outputs, which would re-energise the bridge after a fault.
        if (!enabled)
            return;

        auto position = encoder.Read();
        inverter.ThreePhasePwmOutput(foc.Calculate(currentPhases, position));
    }
}
