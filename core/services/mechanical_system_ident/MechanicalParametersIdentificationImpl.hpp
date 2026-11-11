#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/mechanical_system_ident/MechanicalFit.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/ObservationQueue.hpp"
#include "core/services/mechanical_system_ident/SampleWindow.hpp"
#include "core/services/mechanical_system_ident/TwoLevelSpeedProfile.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/SharedPtr.hpp"
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

        void OnSamplingUpdate(const foc::PhaseCurrents& currentPhases, float torque, float mechanicalAngle);
        void OnWindowClosed(const ClosedWindow& closed);
        void Post(const IdentificationObservation& observation);
        void FitPostedObservations();
        void StartExcitation(const Config& config);
        void StopExcitation();
        void ReleaseDrive();
        void Tick();
        void FailEnvelope();
        void FinishRun();
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

        SampleWindow window;
        ObservationQueue queue;
        MechanicalFit fit;
        TwoLevelSpeedProfile profile;
        infra::TimerRepeating tickTimer;
        infra::TimerSingleShot timeoutTimer;
        volatile Outcome outcome{ Outcome::pending };
    };
}
