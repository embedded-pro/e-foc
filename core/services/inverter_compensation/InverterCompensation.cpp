#include "core/services/inverter_compensation/InverterCompensation.hpp"
#include <chrono>

namespace services
{
    InverterCompensation::InverterCompensation(drivers::ThreePhaseInverter& inverter, const drivers::DeadTimeLoss& loss)
        : inverter(inverter)
        , loss(loss)
        , compensatesDeadTime(loss.effectiveDeadTime.count() != 0 && loss.kneeCurrent.Value() > 0.0f)
        , inverseKneeCurrent(compensatesDeadTime ? 1.0f / loss.kneeCurrent.Value() : 0.0f)
    {}

    void InverterCompensation::ApplyOffsets(const foc::PhaseCurrents& newOffsets)
    {
        offsets = newOffsets;
    }

    void InverterCompensation::PhaseCurrentsReady(hal::Hertz frequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        ScaleLoss(inverter.BaseFrequency());
        onCurrents.Set(onDone);
        inverter.PhaseCurrentsReady(frequency, [this](foc::PhaseCurrents currents)
            {
                Forward(currents);
            });
    }

    void InverterCompensation::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        if (compensatesDeadTime)
            inverter.ThreePhasePwmOutput(Compensated(dutyPhases));
        else
            inverter.ThreePhasePwmOutput(dutyPhases);
    }

    void InverterCompensation::Start()
    {
        inverter.Start();
    }

    void InverterCompensation::Stop()
    {
        inverter.Stop();
    }

    hal::Hertz InverterCompensation::BaseFrequency() const
    {
        return inverter.BaseFrequency();
    }

    foc::Ampere InverterCompensation::MaxCurrentSupported() const
    {
        return inverter.MaxCurrentSupported();
    }

    void InverterCompensation::ScaleLoss(hal::Hertz baseFrequency)
    {
        const auto deadTimeSeconds = std::chrono::duration<float>(loss.effectiveDeadTime).count();
        lossCounts = deadTimeSeconds * static_cast<float>(baseFrequency.Value()) * static_cast<float>(hal::DutyCycle::fullScale);
    }
}
