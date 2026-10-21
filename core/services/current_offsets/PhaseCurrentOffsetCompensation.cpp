#include "core/services/current_offsets/PhaseCurrentOffsetCompensation.hpp"
#include "core/services/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <cmath>

namespace services
{
    PhaseCurrentOffsetCompensation::PhaseCurrentOffsetCompensation(drivers::ThreePhaseInverter& inverter)
        : PhaseCurrentOffsetCompensation(inverter, Config{})
    {}

    PhaseCurrentOffsetCompensation::PhaseCurrentOffsetCompensation(drivers::ThreePhaseInverter& inverter, const Config& config)
        : inverter(inverter)
        , config(config)
    {}

    PhaseCurrentOffsetCompensation::~PhaseCurrentOffsetCompensation()
    {
        noSampleTimer.Cancel();
        if (onMeasured)
            inverter.Stop();
    }

    void PhaseCurrentOffsetCompensation::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        onCorrectedCurrents.Set(onDone);
        inverter.PhaseCurrentsReady(baseFrequency, [this](foc::PhaseCurrents currents)
            {
                onCorrectedCurrents(foc::PhaseCurrents{ currents.a - offsets.a, currents.b - offsets.b, currents.c - offsets.c });
            });
    }

    void PhaseCurrentOffsetCompensation::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        inverter.ThreePhasePwmOutput(dutyPhases);
    }

    void PhaseCurrentOffsetCompensation::Start()
    {
        inverter.Start();
    }

    void PhaseCurrentOffsetCompensation::Stop()
    {
        inverter.Stop();
    }

    hal::Hertz PhaseCurrentOffsetCompensation::BaseFrequency() const
    {
        return inverter.BaseFrequency();
    }

    foc::Ampere PhaseCurrentOffsetCompensation::MaxCurrentSupported() const
    {
        return inverter.MaxCurrentSupported();
    }

    void PhaseCurrentOffsetCompensation::Measure(const infra::Function<void(std::optional<foc::PhaseCurrents>)>& onDone)
    {
        ++run;
        finishing = false;
        measurementComplete = false;
        sums = {};
        settledSamples = 0;
        averagedSamples = 0;
        onMeasured = onDone;

        inverter.PhaseCurrentsReady(inverter.BaseFrequency(), [this](foc::PhaseCurrents currents)
            {
                OnMeasurementSample(currents);
            });
        inverter.ThreePhasePwmOutput(foc::PhasePwmDutyCycles{ neutralDuty, neutralDuty, neutralDuty });

        // The interrupt can trip between registering the callback and the write above, which would re-arm the bridge it stopped.
        if (finishing)
        {
            inverter.Stop();
            return;
        }

        StartSampleWatchdog();
    }

    void PhaseCurrentOffsetCompensation::Apply(const foc::PhaseCurrents& newOffsets)
    {
        offsets = newOffsets;
    }

    bool PhaseCurrentOffsetCompensation::IsRunning() const
    {
        return static_cast<bool>(onMeasured);
    }

    void PhaseCurrentOffsetCompensation::Abort()
    {
        if (!onMeasured)
            return;

        noSampleTimer.Cancel();
        inverter.Stop();
        onMeasured = nullptr;
    }

    void PhaseCurrentOffsetCompensation::OnMeasurementSample(foc::PhaseCurrents currents)
    {
        if (!onMeasured || finishing)
            return;

        sampleSeen = true;

        if (ExceedsInjectionLimit(currents, inverter.MaxCurrentSupported()))
        {
            Finish(false);
            return;
        }

        if (settledSamples != config.settleSamples)
        {
            ++settledSamples;
            return;
        }

        sums.a += currents.a.Value();
        sums.b += currents.b.Value();
        sums.c += currents.c.Value();

        if (++averagedSamples == config.averagedSamples)
            Finish(true);
    }

    void PhaseCurrentOffsetCompensation::StartSampleWatchdog()
    {
        sampleSeen = false;
        noSampleTimer.Start(config.noSampleTimeout, [this]()
            {
                if (!sampleSeen)
                    FailMeasurement();

                sampleSeen = false;
            });
    }

    void PhaseCurrentOffsetCompensation::FailMeasurement()
    {
        if (finishing)
            return;

        noSampleTimer.Cancel();
        inverter.Stop();

        if (onMeasured)
            onMeasured(std::nullopt);
    }

    // Interrupt context: only the bridge stops here; the timer and the caller belong to the event loop, and the run tag drops an aborted run's outcome.
    void PhaseCurrentOffsetCompensation::Finish(bool measured)
    {
        inverter.Stop();
        measurementComplete = measured;
        finishing = true;

        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [finishingRun = run](const infra::SharedPtr<PhaseCurrentOffsetCompensation>& self)
            {
                if (self->run != finishingRun)
                    return;

                self->Complete();
            },
            WeakFromThis());
    }

    void PhaseCurrentOffsetCompensation::Complete()
    {
        noSampleTimer.Cancel();

        if (onMeasured)
            onMeasured(measurementComplete ? MeasuredOffsets() : std::nullopt);
    }

    std::optional<foc::PhaseCurrents> PhaseCurrentOffsetCompensation::MeasuredOffsets() const
    {
        const auto count = static_cast<float>(config.averagedSamples);
        const foc::PhaseCurrents measured{ foc::Ampere{ sums.a / count }, foc::Ampere{ sums.b / count }, foc::Ampere{ sums.c / count } };
        const auto limit = config.maxOffset.Value();

        if (std::fabs(measured.a.Value()) > limit || std::fabs(measured.b.Value()) > limit || std::fabs(measured.c.Value()) > limit)
            return std::nullopt;

        return measured;
    }
}
