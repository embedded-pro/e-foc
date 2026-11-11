#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/cogging_compensation/CoggingMapBuilder.hpp"
#include "core/services/cogging_compensation/CoggingMapping.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/SharedPtr.hpp"

namespace services
{
    class CoggingMappingImpl
        : public CoggingMapping
        , public infra::EnableSharedFromThis<CoggingMappingImpl>
    {
    public:
        CoggingMappingImpl(foc::SpeedCommandable& controller, foc::Controllable& drive, foc::PhaseCurrentsObservable& observable, drivers::ThreePhaseInverter& inverter, drivers::Encoder& encoder);

        void Measure(foc::NewtonMeter torqueConstant, foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian viscousFriction, std::size_t polePairs, const Config& config, const infra::Function<void(const CoggingMeasurement&)>& onDone) override;
        bool IsRunning() const override;
        void Abort() override;

    private:
        enum class Phase : uint8_t
        {
            idle,
            forwardSettle,
            forwardMeasure,
            reversing,
            backwardStart,
            backwardSettle,
            backwardMeasure,
            complete,
            failed
        };

        void OnSamplingUpdate(const foc::PhaseCurrents& currentPhases, float quadratureCurrent, float position);
        void Advance(float electricalAngle, float quadratureCurrent, float rotation);
        float TargetSpeedIn(Phase current) const;
        void StepSpeedCommand();
        void StopMotion();
        void ScheduleFinish();
        void Finish();

        foc::SpeedCommandable& controller;
        foc::Controllable& drive;
        foc::PhaseCurrentsObservable& observable;
        drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        float samplePeriod;
        foc::Ampere supportedCurrent;

        struct Mechanics
        {
            foc::NewtonMeter torqueConstant{ 0.0f };
            foc::NewtonMeterSecondSquared inertia{ 0.0f };
            foc::NewtonMeterSecondPerRadian viscousFriction{ 0.0f };
            std::size_t polePairs{ 0 };
        };

        Config config;
        Mechanics mechanics;
        foc::Ampere currentEnvelope{ 0.0f };
        infra::AutoResetFunction<void(const CoggingMeasurement&)> onDone;

        CoggingMapBuilder builder;

        struct Travel
        {
            float previousPosition{ 0.0f };
            float turned{ 0.0f };
            float mark{ 0.0f };
        };

        Travel travel;
        float commandedSpeed{ 0.0f };
        volatile Phase phase{ Phase::idle };
        uint32_t run{ 0 };

        infra::TimerRepeating slewTimer;
        infra::TimerSingleShot timeoutTimer;
    };
}
