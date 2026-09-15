#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/services/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <cmath>

namespace services
{
    MechanicalParametersIdentificationImpl::MechanicalParametersIdentificationImpl(foc::SpeedCommandable& controller, foc::Controllable& drive, foc::PhaseCurrentsObservable& observable, drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder)
        : controller(controller)
        , drive(drive)
        , observable(observable)
        , inverter(driver)
        , encoder(encoder)
        , samplingPeriod(1.0f / static_cast<float>(driver.BaseFrequency().Value()))
        , supportedCurrent(driver.MaxCurrentSupported())
    {
    }

    void MechanicalParametersIdentificationImpl::EstimateFrictionAndInertia(const foc::NewtonMeter& torqueConstant, std::size_t numberOfPolePairs, const Config& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)>& onDone)
    {
        if (rls.has_value() || !IsUsableIdentificationConfig(config) || !foc::IsFinitePositive(torqueConstant.Value()) || numberOfPolePairs == 0)
        {
            onDone(std::nullopt, std::nullopt);
            return;
        }

        this->currentConfig = config;
        this->currentEnvelope = foc::Ampere{ std::min(config.maxCurrent.Value(), supportedCurrent.Value()) };
        this->onDone = onDone;
        this->previousPosition = encoder.Read().Value();
        this->previousSpeed = 0.0f;
        this->polePairs = static_cast<float>(numberOfPolePairs);
        this->excitedUpdates = 0;
        this->atDwellLevel = false;
        this->outcome = Outcome::pending;

        rls.emplace(1000.0f, config.forgettingFactor);

        observable.RegisterPhaseCurrentsObserver([this, torqueConstant](const auto& currents)
            {
                OnSamplingUpdate(currents, torqueConstant);
            });

        drive.Start();
        controller.EnableSpeedCommand();
        controller.CommandSpeed(config.targetSpeed);

        excitationTimer.Start(config.dwellTime, [this]()
            {
                CommandNextExcitationLevel();
            });

        timeoutTimer.Start(config.timeout, [this]()
            {
                FinishRun();
            });
    }

    void MechanicalParametersIdentificationImpl::CommandNextExcitationLevel()
    {
        atDwellLevel = !atDwellLevel;
        controller.CommandSpeed(atDwellLevel ? currentConfig.dwellSpeed : currentConfig.targetSpeed);
    }

    void MechanicalParametersIdentificationImpl::Abort()
    {
        if (!rls.has_value())
            return;

        StopExcitation();
        ReleaseDrive();
        rls.reset();
        onDone = nullptr;
    }

    bool MechanicalParametersIdentificationImpl::IsRunning() const
    {
        return rls.has_value();
    }

    void MechanicalParametersIdentificationImpl::StopExcitation()
    {
        timeoutTimer.Cancel();
        excitationTimer.Cancel();
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

    void MechanicalParametersIdentificationImpl::ScheduleFinish()
    {
        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [](const infra::SharedPtr<MechanicalParametersIdentificationImpl>& self)
            {
                self->FinishRun();
            },
            WeakFromThis());
    }

    void MechanicalParametersIdentificationImpl::FinishRun()
    {
        if (!rls.has_value())
            return;

        StopExcitation();
        ReleaseDrive();

        const auto& theta = rls->Coefficients();
        const auto inertia = theta.at(1, 0);
        const auto friction = theta.at(2, 0);
        const bool usable = outcome == Outcome::converged && IsPlausibleMechanics(inertia, friction);
        rls.reset();

        if (usable)
            Complete(foc::NewtonMeterSecondPerRadian{ friction }, foc::NewtonMeterSecondSquared{ inertia });
        else
            Complete(std::nullopt, std::nullopt);
    }

    OPTIMIZE_FOR_SPEED
    void MechanicalParametersIdentificationImpl::OnSamplingUpdate(const foc::PhaseCurrents& currentPhases, const foc::NewtonMeter& torqueConstant)
    {
        if (!rls.has_value())
            return;

        auto mechanicalPos = encoder.Read().Value();
        auto speed = foc::detail::PositionWithWrapAround(mechanicalPos - previousPosition) / samplingPeriod;
        auto acceleration = (speed - previousSpeed) / samplingPeriod;

        previousPosition = mechanicalPos;
        previousSpeed = speed;

        // Checked on every sample the drive is still live, a converged one included: the run is only over
        // once the dispatcher has released the drive, so a sample that leaves the envelope in between must
        // still invalidate the estimate. The power stage is stopped here rather than with the rest of the
        // teardown, because leaving it driving an out-of-envelope current until the dispatcher runs is what
        // the envelope exists to prevent. Releasing the observer here would destroy the closure being
        // executed, so that, and the completion, stay deferred.
        if (ExceedsInjectionLimit(currentPhases, currentEnvelope) || std::abs(speed) > currentConfig.maxSpeed.Value())
        {
            if (outcome != Outcome::outsideEnvelope)
            {
                outcome = Outcome::outsideEnvelope;
                inverter.Stop();
                ScheduleFinish();
            }

            return;
        }

        if (outcome != Outcome::pending)
            return;

        if (!IsMechanicallyExciting(acceleration, speed))
            return;

        auto electricalAngle = mechanicalPos * polePairs;
        auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, foc::FastTrigonometry::Cosine(electricalAngle), foc::FastTrigonometry::Sine(electricalAngle));

        MechanicalRls::MakeRegressor(regressor, acceleration, speed);
        torque.at(0, 0) = rotatingFrame.q * torqueConstant.Value();

        auto metrics = rls->Update(regressor, torque);

        if (excitedUpdates != mechanical_estimate::minimumExcitedUpdates)
            ++excitedUpdates;

        if (!HasConvergedMechanics(metrics, excitedUpdates, currentConfig.forgettingFactor))
            return;

        outcome = Outcome::converged;
        ScheduleFinish();
    }
}
