#include "core/foc/instantiations/Runner.hpp"

namespace foc
{
    Runner::Runner(drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder, FocBase& foc)
        : inverter{ inverter }
        , encoder{ encoder }
        , foc{ foc }
    {
        inverter.PhaseCurrentsReady(inverter.BaseFrequency(), [this](auto currentPhases)
            {
                auto position = this->encoder.Read();
                auto dutyCycles = this->foc.Calculate(currentPhases, position);
                this->inverter.ThreePhasePwmOutput(dutyCycles);
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
}
