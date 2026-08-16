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
        inverter.PhaseCurrentsReady(inverter.BaseFrequency(), [this](auto currentPhases)
            {
                OnPhaseCurrents(currentPhases);
            });
    }

    Runner::~Runner()
    {
        Disable();
    }

    void Runner::Enable()
    {
        foc.Enable();
        inverter.Start();
    }

    void Runner::Disable()
    {
        inverter.Stop();
        foc.Disable();
    }

    OPTIMIZE_FOR_SPEED
    void Runner::OnPhaseCurrents(const PhaseCurrents& currentPhases)
    {
        auto position = encoder.Read();
        inverter.ThreePhasePwmOutput(foc.Calculate(currentPhases, position));
    }
}
