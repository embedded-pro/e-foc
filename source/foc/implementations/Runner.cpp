#include "source/foc/implementations/Runner.hpp"

namespace foc
{
    Runner::Runner(MotorDriver& driver, Encoder& encoder, FocBase& foc)
        : driver{ driver }
        , encoder{ encoder }
        , foc{ foc }
    {
        driver.PhaseCurrentsReady(driver.BaseFrequency(), [this](auto currentPhases)
            {
                auto position = this->encoder.Read();
                auto dutyCycles = this->foc.Calculate(currentPhases, position);
                this->driver.ThreePhasePwmOutput(dutyCycles);
            });
    }

    Runner::~Runner()
    {
        Disable();
    }

    void Runner::Enable()
    {
        foc.Enable();
        driver.Start();
    }

    void Runner::Disable()
    {
        driver.Stop();
        foc.Disable();
    }
}
