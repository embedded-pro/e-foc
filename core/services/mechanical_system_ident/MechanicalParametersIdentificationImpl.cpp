#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/injection_current_limit/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "numerical/math/FastTrigonometry.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>

namespace
{
    constexpr infra::Duration tickPeriod{ std::chrono::milliseconds{ 1 } };
    constexpr float tickPeriodSeconds{ 0.001f };
    constexpr float minimumRunDwells{ 2.0f };
}

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
        if (fit.IsRunning() || !IsUsableIdentificationConfig(config) || !foc::IsFinitePositive(torqueConstant.Value()) || numberOfPolePairs == 0)
        {
            onDone(std::nullopt, std::nullopt);
            return;
        }

        currentConfig = config;
        currentEnvelope = foc::Ampere{ std::min(config.maxCurrent.Value(), supportedCurrent.Value() * identificationCurrentMargin) };
        this->onDone = onDone;
        polePairs = static_cast<float>(numberOfPolePairs);
        outcome = Outcome::pending;
        window.Restart(encoder.Read().Value(), samplingPeriod);
        queue.Clear();

        const auto dwellSamples = std::max<std::size_t>(1, static_cast<std::size_t>(std::chrono::duration<float>(config.dwellTime).count() / samplingPeriod + 0.5f));
        fit.Restart(config.forgettingFactor, dwellSamples, static_cast<std::size_t>(minimumRunDwells * static_cast<float>(dwellSamples)));

        observable.RegisterPhaseCurrentsObserver([this, torqueConstant](const auto& currents)
            {
                OnSamplingUpdate(currents, torqueConstant);
            });

        StartExcitation(config);
    }

    void MechanicalParametersIdentificationImpl::StartExcitation(const Config& config)
    {
        drive.Start();
        controller.EnableSpeedCommand();
        controller.CommandSpeed(foc::RadiansPerSecond{ 0.0f });

        tickTimer.Start(tickPeriod, [this]()
            {
                Tick();
            });

        profile.Start(TwoLevelSpeedProfile::Levels{ config.dwellSpeed.Value(), config.targetSpeed.Value(), config.speedSlewRate }, config.dwellTime);

        timeoutTimer.Start(config.timeout, [this]()
            {
                FitPostedObservations();

                if (fit.HasConverged() && outcome == Outcome::pending)
                    outcome = Outcome::converged;

                FinishRun();
            });
    }

    void MechanicalParametersIdentificationImpl::Abort()
    {
        if (!fit.IsRunning())
            return;

        StopExcitation();
        ReleaseDrive();
        fit.Stop();
        onDone = nullptr;
    }

    bool MechanicalParametersIdentificationImpl::IsRunning() const
    {
        return fit.IsRunning();
    }

    bool MechanicalParametersIdentificationImpl::HasConverged() const
    {
        return fit.HasConverged();
    }

    void MechanicalParametersIdentificationImpl::StopExcitation()
    {
        timeoutTimer.Cancel();
        profile.Stop();
        tickTimer.Cancel();
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

    void MechanicalParametersIdentificationImpl::Tick()
    {
        controller.CommandSpeed(foc::RadiansPerSecond{ profile.Step(tickPeriodSeconds) });
        FitPostedObservations();

        if (outcome != Outcome::pending)
            FinishRun();
    }

    void MechanicalParametersIdentificationImpl::FinishRun()
    {
        if (!fit.IsRunning())
            return;

        StopExcitation();
        ReleaseDrive();
        FitPostedObservations();

        const auto estimate = fit.Estimate();
        const bool usable = outcome == Outcome::converged && IsPlausibleMechanics(estimate.inertia, estimate.friction);
        fit.Stop();

        if (usable)
            Complete(foc::NewtonMeterSecondPerRadian{ estimate.friction }, foc::NewtonMeterSecondSquared{ estimate.inertia });
        else
            Complete(std::nullopt, std::nullopt);
    }

    void MechanicalParametersIdentificationImpl::FailEnvelope()
    {
        if (outcome == Outcome::outsideEnvelope)
            return;

        outcome = Outcome::outsideEnvelope;
        inverter.Stop();
    }

    void MechanicalParametersIdentificationImpl::OnSamplingUpdate(const foc::PhaseCurrents& currentPhases, const foc::NewtonMeter& torqueConstant)
    {
        if (!fit.IsRunning())
            return;

        if (ExceedsInjectionLimit(currentPhases, currentEnvelope))
        {
            FailEnvelope();
            return;
        }

        const auto mechanicalPos = encoder.Read().Value();
        const auto electricalAngle = mechanicalPos * polePairs;
        const auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, math::FastCosine(electricalAngle), math::FastSine(electricalAngle));

        if (const auto closed = window.Add(mechanicalPos, rotatingFrame.q * torqueConstant.Value()); closed.has_value())
            OnWindowClosed(*closed);
    }

    void MechanicalParametersIdentificationImpl::OnWindowClosed(const ClosedWindow& closed)
    {
        if (closed.resolutionKnown && closed.hadMean && math::Abs(closed.speed) > currentConfig.maxSpeed.Value())
        {
            FailEnvelope();
            return;
        }

        if (outcome != Outcome::pending)
            return;

        if (fit.IsFinishedAt(closed.sample))
        {
            outcome = Outcome::converged;
            return;
        }

        if (!closed.resolutionKnown || !closed.hadSpeed || !IsMechanicallyObservable(closed.centredSpeed))
            return;

        Post(IdentificationObservation{ closed.acceleration, closed.centredSpeed, closed.centredTorque, closed.sample, closed.windowSamples });
    }

    void MechanicalParametersIdentificationImpl::Post(const IdentificationObservation& observation)
    {
        const auto result = queue.Post(observation);
        if (result == ObservationQueue::PostResult::full)
            outcome = Outcome::observationsDropped;
        else if (result == ObservationQueue::PostResult::halfFull)
            ScheduleFit();
    }

    void MechanicalParametersIdentificationImpl::ScheduleFit()
    {
        infra::EventDispatcherWithWeakPtr::Instance().Schedule([](const infra::SharedPtr<MechanicalParametersIdentificationImpl>& self)
            {
                self->FitPostedObservations();
            },
            WeakFromThis());
    }

    void MechanicalParametersIdentificationImpl::FitPostedObservations()
    {
        while (fit.IsRunning())
        {
            const auto observation = queue.Take();
            if (!observation.has_value())
                return;

            fit.Fit(*observation);
        }
    }
}
