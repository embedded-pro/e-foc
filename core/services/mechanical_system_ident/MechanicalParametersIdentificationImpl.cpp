#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services
{
    MechanicalParametersIdentificationImpl::MechanicalParametersIdentificationImpl(foc::SpeedCommandable& controller, foc::Controllable& drive, foc::PhaseCurrentsObservable& observable, drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder)
        : controller(controller)
        , drive(drive)
        , observable(observable)
        , encoder(encoder)
        , samplingPeriod(1.0f / static_cast<float>(driver.BaseFrequency().Value()))
    {
    }

    void MechanicalParametersIdentificationImpl::EstimateFrictionAndInertia(const foc::NewtonMeter& torqueConstant, std::size_t numberOfPolePairs, const Config& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)>& onDone)
    {
        if (rls.has_value())
        {
            onDone(std::nullopt, std::nullopt);
            return;
        }
        this->currentConfig = config;
        this->onDone = onDone;
        this->previousPosition = encoder.Read().Value();
        this->previousSpeed = 0.0f;
        this->polePairs = static_cast<float>(numberOfPolePairs);
        this->converged = false;

        rls.emplace(1000.0f, config.forgettingFactor);

        // Observe the currents the control loop samples rather than claiming the inverter's single
        // callback slot: taking it leaves Runner::OnPhaseCurrents unreachable, so no duty cycles are
        // ever written, the rotor never turns and the regressor carries no excitation to converge on.
        observable.RegisterPhaseCurrentsObserver([this, torqueConstant](const auto& currents)
            {
                OnSamplingUpdate(currents, torqueConstant);
            });

        drive.Start();
        controller.CommandSpeed(config.targetSpeed);

        timeoutTimer.Start(config.timeout, [this]()
            {
                ReleaseDrive();
                rls.reset();
                Complete(std::nullopt, std::nullopt);
            });
    }

    void MechanicalParametersIdentificationImpl::Abort()
    {
        if (!rls.has_value())
            return;

        timeoutTimer.Cancel();
        ReleaseDrive();
        rls.reset();
        onDone = nullptr;
    }

    void MechanicalParametersIdentificationImpl::ReleaseDrive()
    {
        drive.Stop();
        controller.DisableSpeedCommand();
        observable.UnregisterPhaseCurrentsObserver();
    }

    void MechanicalParametersIdentificationImpl::Complete(std::optional<foc::NewtonMeterSecondPerRadian> friction, std::optional<foc::NewtonMeterSecondSquared> inertia)
    {
        if (onDone)
            onDone(friction, inertia);
    }

    void MechanicalParametersIdentificationImpl::OnSamplingUpdate(const foc::PhaseCurrents& currentPhases, const foc::NewtonMeter& torqueConstant)
    {
        if (converged || !rls.has_value())
            return;

        auto mechanicalPos = encoder.Read().Value();
        auto electricalAngle = mechanicalPos * polePairs;
        auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, foc::FastTrigonometry::Cosine(electricalAngle), foc::FastTrigonometry::Sine(electricalAngle));

        auto speed = foc::detail::PositionWithWrapAround(mechanicalPos - previousPosition) / samplingPeriod;
        auto acceleration = (speed - previousSpeed) / samplingPeriod;
        MotorRLS::MakeRegressor(regressor, acceleration, speed);

        torque.at(0, 0) = rotatingFrame.q * torqueConstant.Value();

        auto metrics = rls->Update(regressor, torque);

        previousPosition = mechanicalPos;
        previousSpeed = speed;

        if (MotorRLS::EvaluateConvergence(metrics, 1e-4f, 1e-2f) != estimators::State::converged)
            return;

        // This runs in the ADC interrupt. Releasing the observer here would destroy the very
        // infra::Function being executed, and the completion reaches non-volatile memory through
        // the state machine, so both are handed to the dispatcher instead.
        converged = true;
        timeoutTimer.Cancel();

        infra::EventDispatcher::Instance().Schedule([this]()
            {
                if (!rls.has_value())
                    return;

                ReleaseDrive();

                auto& theta = rls->Coefficients();
                const auto friction = foc::NewtonMeterSecondPerRadian{ theta.at(2, 0) };
                const auto inertia = foc::NewtonMeterSecondSquared{ theta.at(1, 0) };
                rls.reset();
                Complete(friction, inertia);
            });
    }
}
