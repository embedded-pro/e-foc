#include "core/services/dead_time_compensation/DeadTimeCompensation.hpp"
#include <chrono>

namespace services
{
    DeadTimeCompensation::DeadTimeCompensation(drivers::ThreePhaseInverter& inverter, const drivers::DeadTimeLoss& loss)
        : inverter(inverter)
        , loss(loss)
        , inverseKneeCurrent(loss.kneeCurrent.Value() > 0.0f ? 1.0f / loss.kneeCurrent.Value() : 0.0f)
    {}

    void DeadTimeCompensation::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        ScaleLoss(baseFrequency);
        onCurrents.Set(onDone);
        inverter.PhaseCurrentsReady(baseFrequency, [this](foc::PhaseCurrents currents)
            {
                Forward(currents);
            });
    }

    void DeadTimeCompensation::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        inverter.ThreePhasePwmOutput(Compensated(dutyPhases));
    }

    void DeadTimeCompensation::Start()
    {
        inverter.Start();
    }

    void DeadTimeCompensation::Stop()
    {
        inverter.Stop();
    }

    hal::Hertz DeadTimeCompensation::BaseFrequency() const
    {
        return inverter.BaseFrequency();
    }

    foc::Ampere DeadTimeCompensation::MaxCurrentSupported() const
    {
        return inverter.MaxCurrentSupported();
    }

    void DeadTimeCompensation::ScaleLoss(hal::Hertz baseFrequency)
    {
        const auto deadTimeSeconds = std::chrono::duration<float>(loss.effectiveDeadTime).count();
        lossCounts = deadTimeSeconds * static_cast<float>(baseFrequency.Value()) * static_cast<float>(hal::DutyCycle::fullScale);
    }
}
