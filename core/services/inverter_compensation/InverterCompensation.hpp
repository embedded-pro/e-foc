#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <cstdint>

namespace services
{
    class InverterCompensation
        : public drivers::ThreePhaseInverter
    {
    public:
        InverterCompensation(drivers::ThreePhaseInverter& inverter, const drivers::DeadTimeLoss& loss);

        void ApplyOffsets(const foc::PhaseCurrents& offsets);

        void PhaseCurrentsReady(hal::Hertz frequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

    private:
        OPTIMIZE_FOR_SPEED void Forward(const foc::PhaseCurrents& raw)
        {
            lastCurrents = foc::PhaseCurrents{
                foc::Ampere{ raw.a.Value() - offsets.a.Value() },
                foc::Ampere{ raw.b.Value() - offsets.b.Value() },
                foc::Ampere{ raw.c.Value() - offsets.c.Value() }
            };
            onCurrents(lastCurrents);
        }

        OPTIMIZE_FOR_SPEED hal::DutyCycle Compensated(hal::DutyCycle duty, foc::Ampere current) const
        {
            const auto share = std::clamp(current.Value() * inverseKneeCurrent, -1.0f, 1.0f);
            const auto counts = std::clamp(static_cast<float>(duty.Value()) + share * lossCounts, 0.0f, static_cast<float>(hal::DutyCycle::fullScale));

            return hal::DutyCycle{ static_cast<uint32_t>(counts + 0.5f) };
        }

        OPTIMIZE_FOR_SPEED foc::PhasePwmDutyCycles Compensated(const foc::PhasePwmDutyCycles& duties) const
        {
            return foc::PhasePwmDutyCycles{
                Compensated(duties.a, lastCurrents.a),
                Compensated(duties.b, lastCurrents.b),
                Compensated(duties.c, lastCurrents.c)
            };
        }

        void ScaleLoss(hal::Hertz baseFrequency);

        drivers::ThreePhaseInverter& inverter;
        drivers::DeadTimeLoss loss;
        const bool compensatesDeadTime;
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onCurrents;
        float inverseKneeCurrent{ 0.0f };
        float lossCounts{ 0.0f };
        foc::PhaseCurrents offsets{};
        foc::PhaseCurrents lastCurrents{};
    };
}
