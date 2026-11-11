#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/util/Function.hpp"
#include <chrono>

namespace drivers
{
    struct DeadTimeLoss
    {
        std::chrono::nanoseconds effectiveDeadTime{ 0 };
        foc::Ampere kneeCurrent{ 0.0f };
    };

    class Encoder
    {
    protected:
        ~Encoder() = default;

    public:
        virtual foc::Radians Read() = 0;
        virtual void Set(foc::Radians value) = 0;
        virtual void SetZero() = 0;
    };

    class EncoderIndex
    {
    protected:
        ~EncoderIndex() = default;

    public:
        virtual bool HasIndex() const = 0;
        virtual void ArmIndex() = 0;
        // Polled from the phase-current interrupt; true once a pulse has passed since ArmIndex
        virtual bool IndexSeen() = 0;
    };

    class ThreePhasePowerStage
        : public foc::ZeroVectorSlopeSink
    {
    protected:
        ~ThreePhasePowerStage() = default;

    public:
        virtual void PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) = 0;
        virtual void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) = 0;
        virtual void Start() = 0;
        virtual void Stop() = 0;
        virtual hal::Hertz BaseFrequency() const = 0;
        virtual foc::Ampere MaxCurrentSupported() const = 0;
        // The offsets the stage above removes from every sample; a phase rebuilt from the other two must carry them.
        virtual void CurrentOffsets(const foc::PhaseCurrents& offsets) = 0;
    };

    class ThreePhaseInverter
    {
    protected:
        ~ThreePhaseInverter() = default;

    public:
        virtual void PhaseCurrentsReady(hal::Hertz frequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) = 0;
        virtual void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) = 0;
        virtual void Start() = 0;
        virtual void Stop() = 0;
        virtual hal::Hertz BaseFrequency() const = 0;
        virtual foc::Ampere MaxCurrentSupported() const = 0;
        virtual void CurrentOffsets(const foc::PhaseCurrents& offsets) = 0;
    };
}
