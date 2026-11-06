#include "core/services/index_search/IndexSearchImpl.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/services/electrical_system_ident/NormalizedDutyCycles.hpp"
#include "core/services/injection_current_limit/InjectionCurrentLimit.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace services
{
    IndexSearchImpl::IndexSearchImpl(drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder, drivers::EncoderIndex& index)
        : driver(driver)
        , encoder(encoder)
        , index(index)
    {}

    bool IndexSearchImpl::Available() const
    {
        return index.HasIndex();
    }

    void IndexSearchImpl::Search(const Config& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone)
    {
        if (onSearchDone || !IsUsable(config))
        {
            onDone(std::nullopt);
            return;
        }

        Prepare(config);
        onSearchDone = onDone;

        timeoutTimer.Start(timeout, [this]()
            {
                if (phase == Phase::holding || phase == Phase::turning)
                    Conclude(std::nullopt);
            });

        driver.Stop();
        OutputVector();
        driver.PhaseCurrentsReady(driver.BaseFrequency(), [this](auto currents)
            {
                Sample(currents);
            });
    }

    void IndexSearchImpl::Abort()
    {
        if (!onSearchDone)
            return;

        ++run;
        phase = Phase::idle;
        timeoutTimer.Cancel();
        driver.Stop();
        onSearchDone = nullptr;
    }

    bool IndexSearchImpl::IsRunning() const
    {
        return static_cast<bool>(onSearchDone);
    }

    bool IndexSearchImpl::IsUsable(const Config& config)
    {
        return config.phaseResistance.Value() > 0.0f && config.busVoltage.Value() > 0.0f && config.current.Value() > 0.0f && config.polePairs != 0;
    }

    void IndexSearchImpl::Prepare(const Config& config)
    {
        const auto current = std::min(config.current.Value(), maximumShareOfDriveCurrent * driver.MaxCurrentSupported().Value());
        const auto sampleRate = static_cast<float>(driver.BaseFrequency().Value());

        ++run;
        target = config.target;
        amplitude = std::min(2.0f * current * config.phaseResistance.Value() / config.busVoltage.Value(), maximumDutyFraction);
        tripCurrent = foc::Ampere{ std::min(tripFactor * current, driver.MaxCurrentSupported().Value()) };
        stepPerSample = 2.0f * std::numbers::pi_v<float> * searchRevolutionsPerSecond * static_cast<float>(config.polePairs) / sampleRate;
        holdSamples = static_cast<std::size_t>(std::chrono::duration<float>(holdDuration).count() * sampleRate);
        travelSamples = static_cast<std::size_t>(std::ceil(travelRevolutions / searchRevolutionsPerSecond * sampleRate));
        samples = 0;
        electricalAngle = 0.0f;
        phase = Phase::holding;
    }

    void IndexSearchImpl::Sample(const foc::PhaseCurrents& currents)
    {
        if (phase != Phase::holding && phase != Phase::turning)
            return;

        if (ExceedsInjectionLimit(currents, tripCurrent))
            FailFromInterrupt();
        else if (phase == Phase::holding)
            Hold();
        else
            Turn();
    }

    // A rotor snapping to the held vector may pass the index from the wrong side, so the index is armed only after it
    void IndexSearchImpl::Hold()
    {
        if (++samples < holdSamples)
            return;

        index.ArmIndex();
        samples = 0;
        phase = Phase::turning;
    }

    void IndexSearchImpl::Turn()
    {
        if (index.IndexSeen())
        {
            readingAtIndex = encoder.Read();
            phase = Phase::found;

            infra::EventDispatcherWithWeakPtr::Instance().Schedule(
                [foundRun = run](const infra::SharedPtr<IndexSearchImpl>& self)
                {
                    if (self->run == foundRun && self->phase == Phase::found)
                        self->Reference();
                },
                WeakFromThis());
            return;
        }

        if (samples == travelSamples)
        {
            FailFromInterrupt();
            return;
        }

        ++samples;
        electricalAngle = foc::detail::PositionWithWrapAround(electricalAngle + stepPerSample);
        OutputVector();
    }

    void IndexSearchImpl::OutputVector()
    {
        driver.ThreePhasePwmOutput(detail::NormalizedDutyCycles(
            transforms.Inverse(foc::RotatingFrame{ amplitude, 0.0f }, foc::FastTrigonometry::Cosine(electricalAngle), foc::FastTrigonometry::Sine(electricalAngle))));
    }

    void IndexSearchImpl::FailFromInterrupt()
    {
        driver.Stop();
        phase = Phase::failed;

        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [failedRun = run](const infra::SharedPtr<IndexSearchImpl>& self)
            {
                if (self->run == failedRun && self->phase == Phase::failed)
                    self->Conclude(std::nullopt);
            },
            WeakFromThis());
    }

    // The vector still holds the rotor, and the shift is taken from a reading made just before the encoder is set
    void IndexSearchImpl::Reference()
    {
        if (target)
            encoder.Set(foc::Radians{ foc::detail::PositionWithWrapAround(encoder.Read().Value() - readingAtIndex.Value() + target->Value()) });

        Conclude(readingAtIndex);
    }

    void IndexSearchImpl::Conclude(std::optional<foc::Radians> reading)
    {
        if (phase != Phase::failed)
            driver.Stop();

        phase = Phase::idle;
        timeoutTimer.Cancel();

        if (onSearchDone)
            onSearchDone(reading);
    }
}
