#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/supervision/SupervisedInverter.hpp"
#include "numerical/math/CompilerOptimizations.hpp"

namespace supervision
{
    SupervisedInverter::SupervisedInverter(drivers::ThreePhaseInverter& inverter, foc::ProgressSignal& progress)
        : inverter{ inverter }
        , progress{ progress }
    {}

    void SupervisedInverter::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        inverter.PhaseCurrentsReady(baseFrequency, onDone);
    }

    OPTIMIZE_FOR_SPEED
    void SupervisedInverter::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        inverter.ThreePhasePwmOutput(dutyPhases);
        progress.Signal();
    }

    void SupervisedInverter::Start()
    {
        inverter.Start();
    }

    void SupervisedInverter::Stop()
    {
        inverter.Stop();
    }

    hal::Hertz SupervisedInverter::BaseFrequency() const
    {
        return inverter.BaseFrequency();
    }

    foc::Ampere SupervisedInverter::MaxCurrentSupported() const
    {
        return inverter.MaxCurrentSupported();
    }
}
