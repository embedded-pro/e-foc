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

        bool Started() const;
        uint32_t Starts() const;

    private:
        void Decimate(foc::PhaseCurrents currents);

        drivers::ThreePhasePowerStage& powerStage;
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onDecimated;
        uint32_t samplesPerCallback{ 1 };
        uint32_t samplesSinceCallback{ 0 };
        // A fault raised in an interrupt may stop the stage while the event loop reads this.
        volatile bool started{ false };
        uint32_t starts{ 0 };
    };
}
