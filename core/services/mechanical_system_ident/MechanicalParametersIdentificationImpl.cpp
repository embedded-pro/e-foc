#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/services/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr infra::Duration slewPeriod{ std::chrono::milliseconds{ 1 } };
    constexpr float slewPeriodSeconds{ 0.001f };

    // The speed and acceleration come from differences of encoder positions, which an encoder only gives in counts. A window
    // of T seconds quantises the speed at q/T and the acceleration at q/T^2 for a count of q radians, so the window that keeps
    // the acceleration quantum at a fixed target grows with the square root of the count size: a clean encoder is read every
    // sample, a 4000-count one over several milliseconds.
    constexpr float accelerationQuantumTarget{ 100.0f };
    constexpr std::size_t maximumObservationWindow{ 128 };
    constexpr std::size_t minimumCountSteps{ 8 };

    // An inertia that moves by more than this fraction over one memory of the estimator has not settled
    constexpr float inertiaSettlingTolerance{ 0.1f };
    constexpr std::size_t maximumSettlingInterval{ 2000 };
    constexpr float minimumWindowedForgetting{ 1e-3f };

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
        this->hasPreviousSpeed = false;
        this->hasPreviousTorque = false;
        this->torqueSum = 0.0f;
        this->lastSamplePosition = this->previousPosition;
        this->countAngle = std::numeric_limits<float>::infinity();
        this->countSteps = 0;
        this->windowSamples = 1;
        this->samplesInWindow = 0;
        this->windowPeriod = samplingPeriod;
        this->polePairs = static_cast<float>(numberOfPolePairs);
        this->excitation.Restart();
        this->atDwellLevel = true;
        this->outcome = Outcome::pending;
        ++run;

        RestartEstimator(config.forgettingFactor);

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

    // A single innovation below its bound can be luck, and the covariance trace is dominated by the Coulomb term, so neither says
    // the inertia is known. The inertia is compared with itself one estimator memory earlier, and only a run that has stopped
    // moving it may converge.
    void MechanicalParametersIdentificationImpl::TrackInertiaSettling()
    {
        const auto memoryInUpdates = updateForgetting < 1.0f ? static_cast<std::size_t>(std::ceil(1.0f / (1.0f - updateForgetting))) : maximumSettlingInterval;

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

    // The power stage is stopped here rather than with the rest of the teardown, because leaving it driving an
    // out-of-envelope current until the dispatcher runs is what the envelope exists to prevent. Releasing the
    // observer here would destroy the closure being executed, so that, and the completion, stay deferred.
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

        // The torque is accumulated over the window like the speed is: the acceleration between two window speeds is a triangular
        // average of the true acceleration over both windows, and only a torque averaged the same way can be regressed on it.
        const auto mechanicalPos = encoder.Read().Value();
        const auto electricalAngle = mechanicalPos * polePairs;
        const auto rotatingFrame = transform.Forward(foc::ThreePhase{ currentPhases.a.Value(), currentPhases.b.Value(), currentPhases.c.Value() }, foc::FastTrigonometry::Cosine(electricalAngle), foc::FastTrigonometry::Sine(electricalAngle));
        torqueSum += rotatingFrame.q * torqueConstant.Value();

        // The smallest position step the encoder ever shows is one of its counts
        const auto step = std::abs(foc::detail::PositionWithWrapAround(mechanicalPos - lastSamplePosition));
        lastSamplePosition = mechanicalPos;

        if (step > 0.0f)
        {
            countAngle = std::min(countAngle, step);
            ++countSteps;
        }

        if (++samplesInWindow < windowSamples)
            return;

        samplesInWindow = 0;

        const auto windowTorque = torqueSum / static_cast<float>(windowSamples);
        torqueSum = 0.0f;

        const auto speed = foc::detail::PositionWithWrapAround(mechanicalPos - previousPosition) / windowPeriod;
        const auto acceleration = (speed - previousSpeed) / windowPeriod;
        const auto hadSpeed = hasPreviousSpeed;
        const auto hadTorque = hasPreviousTorque;

        // A single-sample window is the sample itself: speed, acceleration and torque all belong to the same instant. Over a longer
        // window the acceleration between two window speeds, the mean of the two, and the mean of the two window torques belong to
        // the boundary between their windows.
        const auto centred = windowSamples > 1;
        const auto centredSpeed = centred ? 0.5f * (speed + previousSpeed) : speed;
        const auto centredTorque = centred ? 0.5f * (windowTorque + previousTorque) : windowTorque;

        previousPosition = mechanicalPos;
        previousSpeed = speed;
        hasPreviousSpeed = true;
        previousTorque = windowTorque;
        hasPreviousTorque = true;

        // Until enough counts have been seen the window is a guess
        const auto countKnown = countSteps >= minimumCountSteps;

        // A changed window length starts the pairing again
        const auto wanted = countKnown ? WindowSamplesFor(countAngle, samplingPeriod) : windowSamples;

        if (wanted != windowSamples)
        {
            windowSamples = wanted;
            windowPeriod = samplingPeriod * static_cast<float>(wanted);

            // The forgetting factor is a memory per observation, and a window makes an observation span several samples, so
            // the factor is rescaled to keep the memory the same in time rather than in observations
            RestartEstimator(std::max(std::pow(currentConfig.forgettingFactor, static_cast<float>(wanted)), minimumWindowedForgetting));
            hasPreviousSpeed = false;
            hasPreviousTorque = false;
            return;
        }

        // Checked on every observation the drive is still live, a converged one included: the run is only over
        // once the dispatcher has released the drive, so an observation that leaves the envelope in between must
        // still invalidate the estimate.
        if (countKnown && std::abs(speed) > currentConfig.maxSpeed.Value())
        {
            FailEnvelope();
            return;
        }

        if (outcome != Outcome::pending)
            return;

        if (!countKnown || !hadSpeed || !hadTorque || !IsMechanicallyObservable(centredSpeed))
            return;

        MechanicalRls::MakeRegressor(regressor, acceleration, centredSpeed);
        torque.at(0, 0) = centredTorque;

        auto metrics = rls->Update(regressor, torque);
        excitation.Count(acceleration, centredSpeed);
        TrackInertiaSettling();

        if (!inertiaSettled || !HasConvergedMechanics(metrics, excitation, updateForgetting))
            return;

        outcome = Outcome::converged;
        ScheduleFinish();
    }
}
