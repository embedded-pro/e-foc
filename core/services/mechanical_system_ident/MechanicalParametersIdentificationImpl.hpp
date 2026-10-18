#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/mechanical_system_ident/MechanicalEstimatePolicy.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/SharedPtr.hpp"

namespace services
{
    class MechanicalParametersIdentificationImpl
        : public MechanicalParametersIdentification
        , public infra::EnableSharedFromThis<MechanicalParametersIdentificationImpl>
    {
    public:
        MechanicalParametersIdentificationImpl(foc::SpeedCommandable& controller, foc::Controllable& drive, foc::PhaseCurrentsObservable& observable, drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder);

        void EstimateFrictionAndInertia(const foc::NewtonMeter& torqueConstant, std::size_t numberOfPolePairs, const Config& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)>& onDone) override;
        bool IsRunning() const override;
        void Abort() override;

    private:
        enum class Outcome : uint8_t
        {
            pending,
            converged,
            outsideEnvelope
        };

        void OnSamplingUpdate(const foc::PhaseCurrents& currentPhases, const foc::NewtonMeter& torqueConstant);
        void StopExcitation();
        void ReleaseDrive();
        void ScheduleFinish();
        void FailEnvelope();
        void FinishRun();
        void CommandNextExcitationLevel();
        void StepSpeedCommand();
        void RestartEstimator(float forgettingFactor);
        void TrackInertiaSettling();
        void Complete(std::optional<foc::NewtonMeterSecondPerRadian> friction, std::optional<foc::NewtonMeterSecondSquared> inertia);

        foc::SpeedCommandable& controller;
        foc::Controllable& drive;
        foc::PhaseCurrentsObservable& observable;
        drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;

        float samplingPeriod;
        foc::Ampere supportedCurrent;
        Config currentConfig;
        foc::Ampere currentEnvelope{ 0.0f };
        infra::AutoResetFunction<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)> onDone;

        std::optional<MechanicalRls> rls;

        std::size_t windowSamples{ 1 };
        std::size_t samplesInWindow{ 0 };
        float windowPeriod{ 0.0f };
        float updateForgetting{ 1.0f };
        float inertiaReference{ 0.0f };
        std::size_t updatesSinceReference{ 0 };
        bool hasInertiaReference{ false };
        bool inertiaSettled{ false };
        float previousPosition{ 0.0f };
        float previousSpeed{ 0.0f };
        bool hasPreviousSpeed{ false };
        float previousTorque{ 0.0f };
        float torqueSum{ 0.0f };
        float lastSamplePosition{ 0.0f };
        float countAngle{ 0.0f };
        std::size_t countSteps{ 0 };
        bool hasPreviousTorque{ false };
        float polePairs{ 1.0f };
        MechanicalExcitation excitation;
        bool atDwellLevel{ false };
        float commandedSpeed{ 0.0f };
        float levelSpeed{ 0.0f };
        infra::TimerRepeating slewTimer;
        [[no_unique_address]] foc::ClarkePark transform;
        infra::TimerSingleShot timeoutTimer;
        infra::TimerRepeating excitationTimer;
        MechanicalRls::InputMatrix regressor;
        math::Matrix<float, 1, 1> torque;
        volatile Outcome outcome{ Outcome::pending };
        uint32_t run{ 0 };
    };
}
