#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/services/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace
{
    constexpr infra::Duration slewPeriod{ std::chrono::milliseconds{ 1 } };
    constexpr float slewPeriodSeconds{ 0.001f };

    constexpr float accelerationQuantumTarget{ 100.0f };
    constexpr std::size_t maximumObservationWindow{ 256 };
    constexpr std::size_t minimumCountSteps{ 8 };
    constexpr std::size_t stepChangeMemory{ 4096 };
    constexpr float stepChangeToResolution{ std::numbers::sqrt2_v<float> };
    constexpr std::size_t windowHysteresisFloor{ 16 };
    constexpr std::size_t windowHysteresisDivisor{ 16 };

    constexpr float inertiaSettlingTolerance{ 0.1f };
    constexpr std::size_t maximumSettlingInterval{ 2000 };
    constexpr std::size_t minimumSettlingUpdates{ 8 };
    constexpr float coveredWindowSamples{ 16.0f };

    constexpr float minimumRunDwells{ 2.0f };

    std::size_t WindowSamplesFor(float countAngle, float samplingPeriod)
    {
        if (!std::isfinite(countAngle))
            return 1;

        const auto seconds = std::sqrt(countAngle / accelerationQuantumTarget);
        const auto samples = static_cast<std::size_t>(std::ceil(seconds / samplingPeriod));
        return std::clamp<std::size_t>(samples, 1, maximumObservationWindow);
    }
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
        if (rls.has_value() || !IsUsableIdentificationConfig(config) || !foc::IsFinitePositive(torqueConstant.Value()) || numberOfPolePairs == 0)
        {
            onDone(std::nullopt, std::nullopt);
            return;
        }

        this->currentConfig = config;
        this->currentEnvelope = foc::Ampere{ std::min(config.maxCurrent.Value(), supportedCurrent.Value() * identificationCurrentMargin) };
        this->onDone = onDone;
        this->previousPosition = encoder.Read().Value();
        this->previousSpeed = 0.0f;
        this->hasPreviousMean = false;
        this->hasPreviousSpeed = false;
        this->positionSum = 0.0f;
        this->previousMeanToEnd = 0.0f;
        this->torqueSum = 0.0f;
        this->torqueRise = 0.0f;
        this->torquePeak = 0.0f;
        this->torqueFall = 0.0f;
        this->lastSamplePosition = this->previousPosition;
        this->countAngle = std::numeric_limits<float>::infinity();
        this->countSteps = 0;
        this->lastStep = 0.0f;
        this->stepChangeEnergy = 0.0f;
        this->stepChanges = 0;
        this->windowSamples = 1;
        this->samplesInWindow = 0;
        this->windowPeriod = samplingPeriod;
        this->polePairs = static_cast<float>(numberOfPolePairs);
        this->excitation.Restart();
        this->atDwellLevel = true;
        this->outcome = Outcome::pending;
        ++run;

        RestartEstimator(config.forgettingFactor);
        observationsPosted = 0;
        observationsFitted = 0;
        fitScheduled = false;
        this->samplesSinceStart = 0;
        this->dwellSamples = std::max<std::size_t>(1, static_cast<std::size_t>(std::chrono::duration<float>(config.dwellTime).count() / samplingPeriod + 0.5f));
        this->minimumRunSamples = static_cast<std::size_t>(minimumRunDwells * static_cast<float>(dwellSamples));
        this->finishAtSample = 0;

        observable.RegisterPhaseCurrentsObserver([this, torqueConstant](const auto& currents)
            {
                OnSamplingUpdate(currents, torqueConstant);
            });

        this->commandedSpeed = 0.0f;
        this->levelSpeed = config.dwellSpeed.Value();

        drive.Start();
        controller.EnableSpeedCommand();
        controller.CommandSpeed(foc::RadiansPerSecond{ commandedSpeed });

        slewTimer.Start(slewPeriod, [this]()
            {
                StepSpeedCommand();
            });

        excitationTimer.Start(config.dwellTime, [this]()
            {
                CommandNextExcitationLevel();
            });

        timeoutTimer.Start(config.timeout, [this]()
            {
                FitPostedObservations();

                if (finishAtSample != 0 && outcome == Outcome::pending)
                    outcome = Outcome::converged;

                FinishRun();
            });
    }

    void MechanicalParametersIdentificationImpl::RestartEstimator(float forgettingFactor)
    {
        updateForgetting = forgettingFactor;
        rls.emplace(1000.0f, updateForgetting);
        hasInertiaReference = false;
        inertiaSettled = false;
        updatesSinceReference = 0;
    }

    void MechanicalParametersIdentificationImpl::TrackInertiaSettling(uint32_t observedWindowSamples)
    {
        const auto memoryInSamples = updateForgetting < 1.0f ? std::ceil(1.0f / (1.0f - updateForgetting)) : static_cast<float>(maximumSettlingInterval);
        const auto memoryInUpdates = std::max<std::size_t>(minimumSettlingUpdates, static_cast<std::size_t>(std::ceil(memoryInSamples / static_cast<float>(observedWindowSamples))));

        if (++updatesSinceReference < memoryInUpdates)
            return;

        const auto inertia = rls->Coefficients().at(1, 0);
        inertiaSettled = hasInertiaReference && std::abs(inertia - inertiaReference) <= inertiaSettlingTolerance * std::abs(inertia);
        inertiaReference = inertia;
        hasInertiaReference = true;
        updatesSinceReference = 0;
    }

    void MechanicalParametersIdentificationImpl::CommandNextExcitationLevel()
    {
        atDwellLevel = !atDwellLevel;
        levelSpeed = atDwellLevel ? currentConfig.dwellSpeed.Value() : currentConfig.targetSpeed.Value();
    }

    void MechanicalParametersIdentificationImpl::StepSpeedCommand()
    {
        const auto step = currentConfig.speedSlewRate * slewPeriodSeconds;
        const auto remaining = levelSpeed - commandedSpeed;

        commandedSpeed = std::abs(remaining) <= step ? levelSpeed : commandedSpeed + std::copysign(step, remaining);
        controller.CommandSpeed(foc::RadiansPerSecond{ commandedSpeed });
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

    bool MechanicalParametersIdentificationImpl::HasConverged() const
    {
        return finishAtSample != 0;
    }

    void MechanicalParametersIdentificationImpl::StopExcitation()
    {
        timeoutTimer.Cancel();
        excitationTimer.Cancel();
        slewTimer.Cancel();
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

    // The finish is deferred, so an abort and a new run can both happen before it is dispatched. Tagging it
    // with the run that queued it keeps a terminal outcome from completing the run that followed it.
    void MechanicalParametersIdentificationImpl::ScheduleFinish()
    {
        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [finishingRun = run](const infra::SharedPtr<MechanicalParametersIdentificationImpl>& self)
            {
                if (self->run != finishingRun)
                    return;

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
        FitPostedObservations();

        const auto& theta = rls->Coefficients();
        const auto inertia = theta.at(1, 0);
        const auto friction = SettledFriction(theta.at(2, 0));
        const bool usable = outcome == Outcome::converged && IsPlausibleMechanics(inertia, friction);
        rls.reset();

        if (usable)
            Complete(foc::NewtonMeterSecondPerRadian{ friction }, foc::NewtonMeterSecondSquared{ inertia });
        else
            Complete(std::nullopt, std::nullopt);
    }

    void MechanicalParametersIdentificationImpl::FailEnvelope()
    {
        if (outcome == Outcome::outsideEnvelope)
            return;

        outcome = Outcome::outsideEnvelope;
        inverter.Stop();
        ScheduleFinish();
    }

    void MechanicalParametersIdentificationImpl::OnSamplingUpdate(const foc::PhaseCurrents& currentPhases, const foc::NewtonMeter& torqueConstant)
    {
        if (!rls.has_value())
            return;

        if (ExceedsInjectionLimit(currentPhases, currentEnvelope))
        {
            FailEnvelope();
            return;
        }

        ++samplesSinceStart;

        const auto mechanicalPos = encoder.Read().Value();
        const auto electricalAngle = mechanicalPos * polePairs;
        const auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, foc::FastTrigonometry::Cosine(electricalAngle), foc::FastTrigonometry::Sine(electricalAngle));

        const auto sampleTorque = rotatingFrame.q * torqueConstant.Value();
        const auto weight = (static_cast<float>(samplesInWindow) + 0.5f) / static_cast<float>(windowSamples);
        torqueSum += sampleTorque;
        torqueRise += sampleTorque * 0.5f * weight * weight;
        torquePeak += sampleTorque * (0.5f + weight * (1.0f - weight));
        torqueFall += sampleTorque * 0.5f * (1.0f - weight) * (1.0f - weight);
        positionSum += foc::detail::PositionWithWrapAround(mechanicalPos - previousPosition);

        const auto signedStep = foc::detail::PositionWithWrapAround(mechanicalPos - lastSamplePosition);
        const auto step = std::abs(signedStep);
        lastSamplePosition = mechanicalPos;

        if (samplesSinceStart > 1)
        {
            const auto change = signedStep - lastStep;
            ++stepChanges;
            stepChangeEnergy += (change * change - stepChangeEnergy) / static_cast<float>(std::min(stepChanges, stepChangeMemory));
        }

        lastStep = signedStep;

        if (step > 0.0f)
        {
            countAngle = std::min(countAngle, step);
            ++countSteps;
        }

        if (++samplesInWindow < windowSamples)
            return;

        samplesInWindow = 0;

        const auto windowTorque = torqueSum / static_cast<float>(windowSamples);
        const auto windowRise = torqueRise;
        const auto windowPeak = torquePeak;
        const auto windowFall = torqueFall;
        const auto meanOffset = positionSum / static_cast<float>(windowSamples);
        torqueSum = 0.0f;
        torqueRise = 0.0f;
        torquePeak = 0.0f;
        torqueFall = 0.0f;
        positionSum = 0.0f;

        const auto speed = (previousMeanToEnd + meanOffset) / windowPeriod;
        const auto acceleration = (speed - previousSpeed) / windowPeriod;
        const auto hadMean = hasPreviousMean;
        const auto hadSpeed = hasPreviousSpeed;

        const auto centred = windowSamples > 1;
        const auto centredSpeed = centred ? 0.5f * (speed + previousSpeed) : speed;
        const auto centredTorque = centred ? (earlierTorqueRise + previousTorquePeak + windowFall) / static_cast<float>(windowSamples) : windowTorque;

        previousMeanToEnd = foc::detail::PositionWithWrapAround(mechanicalPos - previousPosition) - meanOffset;
        previousPosition = mechanicalPos;
        previousSpeed = speed;
        hasPreviousSpeed = hadMean;
        hasPreviousMean = true;
        earlierTorqueRise = previousTorqueRise;
        previousTorqueRise = windowRise;
        previousTorquePeak = windowPeak;

        const auto countKnown = countSteps >= minimumCountSteps;

        const auto stepChangeResolution = stepChanges >= minimumCountSteps ? stepChangeToResolution * std::sqrt(stepChangeEnergy) : 0.0f;
        const auto wanted = countKnown ? WindowSamplesFor(std::max(countAngle, stepChangeResolution), samplingPeriod) : windowSamples;

        const auto windowDifference = wanted > windowSamples ? wanted - windowSamples : windowSamples - wanted;

        if (windowDifference != 0 && (windowSamples < windowHysteresisFloor || windowDifference * windowHysteresisDivisor > windowSamples))
        {
            windowSamples = wanted;
            windowPeriod = samplingPeriod * static_cast<float>(wanted);
            hasPreviousMean = false;
            hasPreviousSpeed = false;
            return;
        }

        if (countKnown && hadMean && std::abs(speed) > currentConfig.maxSpeed.Value())
        {
            FailEnvelope();
            return;
        }

        if (outcome != Outcome::pending)
            return;

        if (finishAtSample != 0 && samplesSinceStart >= finishAtSample)
        {
            outcome = Outcome::converged;
            ScheduleFinish();
            return;
        }

        if (!countKnown || !hadSpeed || !IsMechanicallyObservable(centredSpeed))
            return;

        Post(Observation{ acceleration, centredSpeed, centredTorque, static_cast<uint32_t>(samplesSinceStart), static_cast<uint32_t>(windowSamples) });
    }

    // The fit costs more than the rest of the control interrupt, so the interrupt only hands each window over
    void MechanicalParametersIdentificationImpl::Post(const Observation& observation)
    {
        const auto posted = observationsPosted.load(std::memory_order_relaxed);

        if (posted - observationsFitted.load(std::memory_order_acquire) == observationCapacity)
        {
            outcome = Outcome::observationsDropped;
            ScheduleFinish();
            return;
        }

        observations[posted % observationCapacity] = observation;
        observationsPosted.store(posted + 1, std::memory_order_release);

        if (!fitScheduled.exchange(true))
            ScheduleFit();
    }

    void MechanicalParametersIdentificationImpl::ScheduleFit()
    {
        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [fittingRun = run](const infra::SharedPtr<MechanicalParametersIdentificationImpl>& self)
            {
                if (self->run != fittingRun)
                    return;

                self->FitPostedObservations();
            },
            WeakFromThis());
    }

    void MechanicalParametersIdentificationImpl::FitPostedObservations()
    {
        fitScheduled = false;

        auto fitted = observationsFitted.load(std::memory_order_relaxed);
        while (rls.has_value() && fitted != observationsPosted.load(std::memory_order_acquire))
        {
            Fit(observations[fitted % observationCapacity]);
            observationsFitted.store(++fitted, std::memory_order_release);
        }
    }

    void MechanicalParametersIdentificationImpl::Fit(const Observation& observation)
    {
        // The interrupt finishes on the first window it sees past the boundary, which can be later than this fit set it
        if (finishAtSample != 0 && observation.sample >= finishAtSample)
            return;

        MechanicalRls::MakeRegressor(regressor, observation.acceleration, observation.speed);
        torque.at(0, 0) = observation.torque;

        auto metrics = rls->Update(regressor, torque);
        excitation.Count(observation.acceleration, observation.speed);
        TrackInertiaSettling(observation.windowSamples);

        if (finishAtSample != 0 || observation.sample < minimumRunSamples || !inertiaSettled || !HasConvergedMechanics(metrics, excitation, updateForgetting, std::max(1.0f, static_cast<float>(observation.windowSamples) / coveredWindowSamples)))
            return;

        // An angle-dependent torque cancels only over whole turns, so a converged fit keeps every dwell it started whole
        finishAtSample = (observation.sample / dwellSamples + 1) * dwellSamples;
    }
}
