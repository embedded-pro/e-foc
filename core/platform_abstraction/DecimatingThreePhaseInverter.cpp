#include "core/platform_abstraction/DecimatingThreePhaseInverter.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace application
{
    DecimatingThreePhaseInverter::DecimatingThreePhaseInverter(drivers::ThreePhasePowerStage& powerStage)
        : powerStage(powerStage)
    {}

    void DecimatingThreePhaseInverter::PhaseCurrentsReady(hal::Hertz frequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        const auto base = powerStage.BaseFrequency().Value();
        really_assert(frequency.Value() != 0 && base % frequency.Value() == 0);

        if (frequency.Value() == base)
        {
            powerStage.PhaseCurrentsReady(onDone);
            return;
        }

        onDecimated.Set(onDone);
        samplesPerCallback = base / frequency.Value();
        samplesSinceCallback = 0;
        powerStage.PhaseCurrentsReady([this](foc::PhaseCurrents currents)
            {
                Decimate(currents);
            });
    }

    void DecimatingThreePhaseInverter::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        powerStage.ThreePhasePwmOutput(dutyPhases);
    }

    void DecimatingThreePhaseInverter::Start()
    {
        powerStage.Start();
    }

    void DecimatingThreePhaseInverter::Stop()
    {
        powerStage.Stop();
    }

    hal::Hertz DecimatingThreePhaseInverter::BaseFrequency() const
    {
        return powerStage.BaseFrequency();
    }

    foc::Ampere DecimatingThreePhaseInverter::MaxCurrentSupported() const
    {
        return powerStage.MaxCurrentSupported();
    }

    void DecimatingThreePhaseInverter::Decimate(foc::PhaseCurrents currents)
    {
        if (++samplesSinceCallback < samplesPerCallback)
            return;

        samplesSinceCallback = 0;
        onDecimated(currents);
    }
}
