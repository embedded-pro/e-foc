#pragma once

#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include <cstdint>

namespace application
{
    class DecimatingThreePhaseInverter
        : public drivers::ThreePhaseInverter
    {
    public:
        explicit DecimatingThreePhaseInverter(drivers::ThreePhasePowerStage& powerStage);

        void PhaseCurrentsReady(hal::Hertz frequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

    private:
        void Decimate(foc::PhaseCurrents currents);

        drivers::ThreePhasePowerStage& powerStage;
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onDecimated;
        uint32_t samplesPerCallback{ 1 };
        uint32_t samplesSinceCallback{ 0 };
    };
}
