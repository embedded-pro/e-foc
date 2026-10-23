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
#include <array>
#include <atomic>
#include <cstdint>

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

        bool HasConverged() const;

    private:
        enum class Outcome : uint8_t
        {
            pending,
            converged,
            outsideEnvelope,
            observationsDropped
        };

        struct Observation
        {
            float acceleration{ 0.0f };
            float speed{ 0.0f };
            float torque{ 0.0f };
            uint32_t sample{ 0 };
            uint32_t windowSamples{ 1 };
        };

        static constexpr uint32_t observationCapacity{ 16 };

        void OnSamplingUpdate(const foc::PhaseCurrents& currentPhases, const foc::NewtonMeter& torqueConstant);
        void Post(const Observation& observation);
        void ScheduleFit();
        void FitPostedObservations();
        void Fit(const Observation& observation);
        void StopExcitation();
        void ReleaseDrive();
        void Tick();
        void FailEnvelope();
        void FinishRun();
        void CommandNextExcitationLevel();
        void StepSpeedCommand();
        void RestartEstimator(float forgettingFactor);
        void TrackInertiaSettling(uint32_t observedWindowSamples);
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
        std::size_t samplesSinceStart{ 0 };
        std::size_t minimumRunSamples{ 0 };
        std::size_t dwellSamples{ 1 };
        volatile std::size_t finishAtSample{ 0 };
        float previousPosition{ 0.0f };
        float previousSpeed{ 0.0f };
        bool hasPreviousMean{ false };
        bool hasPreviousSpeed{ false };
        float positionSum{ 0.0f };
        float previousMeanToEnd{ 0.0f };
        float earlierTorqueRise{ 0.0f };
        float previousTorqueRise{ 0.0f };
        float previousTorquePeak{ 0.0f };
        float torqueSum{ 0.0f };
        float torqueRise{ 0.0f };
        float torquePeak{ 0.0f };
        float torqueFall{ 0.0f };
        float lastSamplePosition{ 0.0f };
        float countAngle{ 0.0f };
        std::size_t countSteps{ 0 };
        float lastStep{ 0.0f };
        float stepChangeEnergy{ 0.0f };
        std::size_t stepChanges{ 0 };
        float polePairs{ 1.0f };
        MechanicalExcitation excitation;
        bool atDwellLevel{ false };
        float commandedSpeed{ 0.0f };
        float levelSpeed{ 0.0f };
        infra::TimerRepeating tickTimer;
        [[no_unique_address]] foc::ClarkePark transform;
        infra::TimerSingleShot timeoutTimer;
        infra::TimerRepeating excitationTimer;
        MechanicalRls::InputMatrix regressor;
        math::Matrix<float, 1, 1> torque;
        std::array<Observation, observationCapacity> observations{};
        std::atomic<uint32_t> observationsPosted{ 0 };
        std::atomic<uint32_t> observationsFitted{ 0 };
        volatile Outcome outcome{ Outcome::pending };
    };
}
