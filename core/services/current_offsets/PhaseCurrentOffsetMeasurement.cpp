#include "core/services/current_offsets/PhaseCurrentOffsetMeasurement.hpp"
#include "core/services/injection_current_limit/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <algorithm>
#include <cmath>

namespace services
{
    PhaseCurrentOffsetMeasurement::PhaseCurrentOffsetMeasurement(drivers::ThreePhaseInverter& inverter, InverterCompensation& compensation)
        : PhaseCurrentOffsetMeasurement(inverter, compensation, Config{})
    {}

    PhaseCurrentOffsetMeasurement::PhaseCurrentOffsetMeasurement(drivers::ThreePhaseInverter& inverter, InverterCompensation& compensation, const Config& config)
        : inverter(inverter)
        , compensation(compensation)
        , config(config)
    {}

    PhaseCurrentOffsetMeasurement::~PhaseCurrentOffsetMeasurement()
    {
        noSampleTimer.Cancel();
        if (onMeasured)
            inverter.Stop();
    }

    void PhaseCurrentOffsetMeasurement::Measure(const infra::Function<void(std::optional<foc::PhaseCurrents>)>& onDone)
    {
        ++run;
        finishing = false;
        measurementComplete = false;
        sums = {};
        squares = {};
        noise = foc::Ampere{ 0.0f };
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

    void PhaseCurrentOffsetMeasurement::Apply(const foc::PhaseCurrents& offsets)
    {
        compensation.ApplyOffsets(offsets);
    }

    foc::Ampere PhaseCurrentOffsetMeasurement::Noise() const
    {
        return noise;
    }

    bool PhaseCurrentOffsetMeasurement::IsRunning() const
    {
        return static_cast<bool>(onMeasured);
    }

    void PhaseCurrentOffsetMeasurement::Abort()
    {
        if (!onMeasured)
            return;

        noSampleTimer.Cancel();
        inverter.Stop();
        onMeasured = nullptr;
    }

    void PhaseCurrentOffsetMeasurement::OnMeasurementSample(foc::PhaseCurrents currents)
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
        squares.a += currents.a.Value() * currents.a.Value();
        squares.b += currents.b.Value() * currents.b.Value();
        squares.c += currents.c.Value() * currents.c.Value();

        if (++averagedSamples == config.averagedSamples)
            Finish(true);
    }

    void PhaseCurrentOffsetMeasurement::StartSampleWatchdog()
    {
        sampleSeen = false;
        noSampleTimer.Start(config.noSampleTimeout, [this]()
            {
                if (!sampleSeen)
                    FailMeasurement();

                sampleSeen = false;
            });
    }

    void PhaseCurrentOffsetMeasurement::FailMeasurement()
    {
        if (finishing)
            return;

        noSampleTimer.Cancel();
        inverter.Stop();

        if (onMeasured)
            onMeasured(std::nullopt);
    }

    // Interrupt context: only the stop runs here; the run tag drops an aborted run's outcome.
    void PhaseCurrentOffsetMeasurement::Finish(bool measured)
    {
        inverter.Stop();
        measurementComplete = measured;
        finishing = true;

        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [finishingRun = run](const infra::SharedPtr<PhaseCurrentOffsetMeasurement>& self)
            {
                if (self->run != finishingRun)
                    return;

                self->Complete();
            },
            WeakFromThis());
    }

    void PhaseCurrentOffsetMeasurement::Complete()
    {
        noSampleTimer.Cancel();

        if (onMeasured)
            onMeasured(measurementComplete ? MeasuredOffsets() : std::nullopt);
    }

    std::optional<foc::PhaseCurrents> PhaseCurrentOffsetMeasurement::MeasuredOffsets()
    {
        const auto count = static_cast<float>(config.averagedSamples);
        const foc::PhaseCurrents measured{ foc::Ampere{ sums.a / count }, foc::Ampere{ sums.b / count }, foc::Ampere{ sums.c / count } };
        const auto limit = config.maxOffset.Value();

        if (std::fabs(measured.a.Value()) > limit || std::fabs(measured.b.Value()) > limit || std::fabs(measured.c.Value()) > limit)
            return std::nullopt;

        const auto variance = [count](float sum, float square)
        {
            const auto mean = sum / count;
            return std::max(square / count - mean * mean, 0.0f);
        };
        noise = foc::Ampere{ std::sqrt((variance(sums.a, squares.a) + variance(sums.b, squares.b) + variance(sums.c, squares.c)) / 3.0f) };

        return measured;
    }
}
