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
        void FinishRun();
        void CommandNextExcitationLevel();
        void Complete(std::optional<foc::NewtonMeterSecondPerRadian> friction, std::optional<foc::NewtonMeterSecondSquared> inertia);

        foc::SpeedCommandable& controller;
        foc::Controllable& drive;
        foc::PhaseCurrentsObservable& observable;
        drivers::Encoder& encoder;

        float samplingPeriod;
        foc::Ampere supportedCurrent;
        Config currentConfig;
        foc::Ampere currentEnvelope{ 0.0f };
        infra::AutoResetFunction<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)> onDone;

        std::optional<MechanicalRls> rls;

        float previousPosition{ 0.0f };
        float previousSpeed{ 0.0f };
        float polePairs{ 1.0f };
        uint16_t excitedUpdates{ 0 };
        bool atDwellLevel{ false };
        [[no_unique_address]] foc::ClarkePark transform;
        infra::TimerSingleShot timeoutTimer;
        infra::TimerRepeating excitationTimer;
        MechanicalRls::InputMatrix regressor;
        math::Matrix<float, 1, 1> torque;
        volatile Outcome outcome{ Outcome::pending };
    };
}
