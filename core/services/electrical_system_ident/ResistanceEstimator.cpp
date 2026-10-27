#include "core/services/electrical_system_ident/ResistanceEstimator.hpp"
#include "core/foc/transforms/SpaceVectorModulation.hpp"
#include "core/services/injection_current_limit/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <numeric>

namespace
{
    float AverageAndRemoveFront(infra::BoundedDeque<float>& deque)
    {
        float sum = 0.0f;
        for (const auto& s : deque)
            sum += s;
        const float avg = sum / static_cast<float>(deque.size());
        deque.pop_front();
        return avg;
    }

    float GetSteadyStateCurrent(const infra::BoundedVector<float>& samples)
    {
        const auto lastQuarter = static_cast<std::size_t>(static_cast<float>(samples.size()) * 0.9f);
        return std::accumulate(samples.begin() + lastQuarter, samples.end(), 0.0f) / static_cast<float>(samples.size() - lastQuarter);
    }

    foc::PhasePwmDutyCycles AroundMidRail(hal::DutyCycle testVoltage)
    {
        const auto step = foc::DutyFraction(testVoltage);

        return foc::ToDutyCycles(foc::ThreePhase{ 0.5f + step * (2.0f / 3.0f), 0.5f - step / 3.0f, 0.5f - step / 3.0f });
    }
}

namespace services
{
    ResistanceEstimator::ResistanceEstimator(drivers::ThreePhaseInverter& driver, foc::Volts vdc)
        : driver(driver)
        , vdc(vdc)
    {}

    ResistanceEstimator::~ResistanceEstimator()
    {
        settleTimer.Cancel();
        noSampleTimer.Cancel();
        if (onDone)
        {
            finishing = true;
            driver.Stop();
        }
    }

    void ResistanceEstimator::Start(const Config& config, const infra::Function<void(Result)>& onDone)
    {
        activeConfig = config;
        heldDuties = AroundMidRail(config.testVoltage);
        ++run;
        finishing = false;
        currentSamples.clear();
        filteredSamples.clear();
        this->onDone = onDone;
        StartSettlePhase();
    }

    void ResistanceEstimator::StartSettlePhase()
    {
        // The PWM's own rate: a slower one re-clocks the target's PWM but only decimates the emulator's.
        driver.PhaseCurrentsReady(driver.BaseFrequency(), [this](auto currents)
            {
                if (!this->onDone || finishing)
                    return;

                sampleSeen = true;

                if (ExceedsInjectionLimit(currents, driver.MaxCurrentSupported()))
                    Finish(false);
                else
                    driver.ThreePhasePwmOutput(heldDuties);
            });
        driver.ThreePhasePwmOutput(heldDuties);

        // The interrupt can trip between registering the callback and the write above, which would re-arm the bridge it stopped.
        if (finishing)
        {
            driver.Stop();
            return;
        }

        StartSampleWatchdog();
        settleTimer.Start(activeConfig.settleTime, [this]()
            {
                if (finishing)
                    return;

                StartMeasurementPhase();
            });
    }

    void ResistanceEstimator::StartMeasurementPhase()
    {
        StartSampleWatchdog();
        driver.PhaseCurrentsReady(driver.BaseFrequency(), [this](auto currents)
            {
                OnMeasurementSample(currents);
            });
    }

    void ResistanceEstimator::OnMeasurementSample(foc::PhaseCurrents currents)
    {
        if (!onDone || finishing)
            return;

        sampleSeen = true;

        if (ExceedsInjectionLimit(currents, driver.MaxCurrentSupported()))
        {
            Finish(false);
            return;
        }

        currentSamples.push_back(currents.a.Value());

        if (currentSamples.full())
            filteredSamples.push_back(AverageAndRemoveFront(currentSamples));

        if (filteredSamples.full())
            Finish(true);
        else
            driver.ThreePhasePwmOutput(heldDuties);
    }

    void ResistanceEstimator::Abort()
    {
        if (!onDone)
            return;

        // Ended before the bridge stops: a sample taken in between would write the held duties and re-arm it
        finishing = true;
        settleTimer.Cancel();
        noSampleTimer.Cancel();
        driver.Stop();
        onDone = nullptr;
    }

    void ResistanceEstimator::StartSampleWatchdog()
    {
        sampleSeen = false;
        noSampleTimer.Start(activeConfig.noSampleTimeout, [this]()
            {
                if (!sampleSeen)
                    FailMeasurement();

                sampleSeen = false;
            });
    }

    void ResistanceEstimator::FailMeasurement()
    {
        if (finishing)
            return;

        finishing = true;
        settleTimer.Cancel();
        noSampleTimer.Cancel();
        driver.Stop();

        if (onDone)
            onDone(Result{});
    }

    // Interrupt context: only the stop runs here; the run tag drops an aborted run's outcome.
    void ResistanceEstimator::Finish(bool measured)
    {
        driver.Stop();
        measurementComplete = measured;
        finishing = true;

        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [finishingRun = run](const infra::SharedPtr<ResistanceEstimator>& self)
            {
                if (self->run != finishingRun)
                    return;

                self->Complete();
            },
            WeakFromThis());
    }

    void ResistanceEstimator::Complete()
    {
        settleTimer.Cancel();
        noSampleTimer.Cancel();

        if (onDone)
            onDone(measurementComplete ? Measure() : Result{});
    }

    ResistanceEstimator::Result ResistanceEstimator::Measure() const
    {
        const float steadyStateCurrent = GetSteadyStateCurrent(filteredSamples);

        if (steadyStateCurrent <= 0.0f)
            return Result{};

        const auto appliedDuty = foc::DutyFraction(heldDuties.a) - foc::DutyFraction(heldDuties.b);
        const float terminalVoltage = appliedDuty * vdc.Value();
        const float terminalFactor = activeConfig.windingConfig == WindingConfiguration::Delta
                                         ? deltaTerminalFactor
                                         : wyeTerminalFactor;
        const float phaseResistance = terminalVoltage / steadyStateCurrent / terminalFactor;

        return Result{ foc::Ohm{ phaseResistance } };
    }
}
