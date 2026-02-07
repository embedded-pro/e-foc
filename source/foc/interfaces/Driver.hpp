#pragma once

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/util/Function.hpp"
#include "source/foc/interfaces/Units.hpp"

namespace foc
{
    struct PhaseCurrents
    {
        Ampere a;
        Ampere b;
        Ampere c;
    };

    struct PhasePwmDutyCycles
    {
        hal::Percent a;
        hal::Percent b;
        hal::Percent c;
    };

    enum class Direction : uint8_t
    {
        forward,
        reverse,
    };

    class LowPriorityInterrupt
    {
    public:
        virtual void Trigger() = 0;
        virtual void Register(const infra::Function<void()>& handler) = 0;
    };

    class Encoder
    {
    public:
        virtual Radians Read() = 0;
        virtual void Set(Radians value) = 0;
        virtual void SetZero() = 0;
    };

    class HallSensor
    {
    public:
        virtual std::pair<HallState, Direction> Read() const = 0;
    };

    class MotorDriver
    {
    public:
        virtual void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(PhaseCurrents currentPhases)>& onDone) = 0;
        virtual void ThreePhasePwmOutput(const PhasePwmDutyCycles& dutyPhases) = 0;
        virtual void Start() = 0;
        virtual void Stop() = 0;
        virtual hal::Hertz BaseFrequency() const = 0;
    };
}
