#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"

namespace supervision
{
    class SupervisedInverter
        : public drivers::ThreePhaseInverter
    {
    public:
        SupervisedInverter(drivers::ThreePhaseInverter& inverter, foc::ProgressSignal& progress);

        // Implementation of drivers::ThreePhaseInverter
        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

    private:
        drivers::ThreePhaseInverter& inverter;
        foc::ProgressSignal& progress;
    };
}
