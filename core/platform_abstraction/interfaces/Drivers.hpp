#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/util/Function.hpp"

namespace drivers
{
    class Encoder
    {
    public:
        virtual foc::Radians Read() = 0;
        virtual void Set(foc::Radians value) = 0;
        virtual void SetZero() = 0;
    };

    class HallSensor
    {
    public:
        virtual std::pair<foc::HallState, foc::Direction> Read() const = 0;
    };

    class ThreePhaseInverter
    {
    public:
        virtual void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) = 0;
        virtual void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) = 0;
        virtual void Start() = 0;
        virtual void Stop() = 0;
        virtual hal::Hertz BaseFrequency() const = 0;
        virtual foc::Ampere MaxCurrentSupported() const = 0;
    };
}
