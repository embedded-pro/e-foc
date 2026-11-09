#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>

namespace foc
{
    // Keeps the line-to-line voltages while lowering the duties so two low-side shunts stay readable.
    ALWAYS_INLINE_HOT PhasePwmDutyCycles KeepLowSideShuntsReadable(const PhasePwmDutyCycles& duties, hal::DutyCycle maxReadable, hal::DutyCycle maxDuty)
    {
        const auto limit = maxReadable.Value();
        const auto highest = std::max({ duties.a.Value(), duties.b.Value(), duties.c.Value() });

        if (highest <= limit)
            return duties;

        const auto lowest = std::min({ duties.a.Value(), duties.b.Value(), duties.c.Value() });
        const auto middle = duties.a.Value() + duties.b.Value() + duties.c.Value() - highest - lowest;
        const auto shift = std::min(highest - limit, lowest);
        const auto shiftedMiddle = middle - shift;
        const auto shiftedHighest = highest - shift;

        uint32_t bound = 1;
        uint32_t value = 1;
        if (shiftedMiddle > limit)
        {
            bound = limit;
            value = shiftedMiddle;
        }
        if (shiftedHighest > maxDuty.Value() && uint64_t{ maxDuty.Value() } * value < uint64_t{ bound } * shiftedHighest)
        {
            bound = maxDuty.Value();
            value = shiftedHighest;
        }
        const auto scale = value != bound ? static_cast<float>(bound) / static_cast<float>(value) : 1.0f;

        const auto keep = [shift, scale](hal::DutyCycle duty)
        {
            return hal::DutyCycle{ static_cast<uint32_t>(static_cast<float>(duty.Value() - shift) * scale + 0.5f) };
        };

        return PhasePwmDutyCycles{ keep(duties.a), keep(duties.b), keep(duties.c) };
    }

    struct SequentialConverter
    {
        float converterClockHertz{ 0.0f };
        uint32_t sampleHoldClocks{ 0 };
        uint32_t conversionClocks{ 0 };
        uint32_t averagedConversions{ 1 };
        std::chrono::duration<float> triggerDelay{ 0.0f };
    };

    // One converter reading A, B, C in turn from the window centre; a step behaves as held at the mean of its conversions.
    constexpr PhaseSampleDelays SequentialPhaseSampleDelays(const SequentialConverter& converter)
    {
        if (converter.converterClockHertz <= 0.0f)
            return {};

        const auto averaged = static_cast<float>(std::max(converter.averagedConversions, uint32_t{ 1 }));
        const auto clocksPerConversion = static_cast<float>(converter.sampleHoldClocks + converter.conversionClocks);
        const auto step = [&converter, averaged, clocksPerConversion](float index)
        {
            const auto meanConversion = index * averaged + 0.5f * (averaged - 1.0f);
            const auto clocks = meanConversion * clocksPerConversion + static_cast<float>(converter.sampleHoldClocks);
            return converter.triggerDelay + std::chrono::duration<float>{ clocks / converter.converterClockHertz };
        };

        return { step(0.0f), step(1.0f), step(2.0f) };
    }

    // A phase is unreadable if either period around the PWM centre gave it a duty above the limit.
    class LowSideShuntReadout
    {
    public:
        explicit LowSideShuntReadout(hal::DutyCycle maxReadable)
            : maxReadable{ maxReadable.Value() }
        {}

        ALWAYS_INLINE_HOT void Applied(const PhasePwmDutyCycles& duties)
        {
            previous = latest;
            latest = duties;
        }

        void Offsets(const PhaseCurrents& offsets)
        {
            offsetSum = offsets.a.Value() + offsets.b.Value() + offsets.c.Value();
        }

        void SampleDelays(const PhaseSampleDelays& sampleDelays)
        {
            delays = sampleDelays;
        }

        // From the control interrupt, for the next sample; see documentation/design/platform-sampling-skew.md.
        ALWAYS_INLINE_HOT void ZeroVectorSlopes(const PhaseCurrentSlopes& slopes)
        {
            shift.a = slopes.a.Value() * delays.a.count();
            shift.b = slopes.b.Value() * delays.b.count();
            shift.c = slopes.c.Value() * delays.c.count();
        }

        void ClearSlopes()
        {
            shift = {};
        }

        ALWAYS_INLINE_HOT PhaseCurrents Read(const PhaseCurrents& sampled) const
        {
            const PhaseCurrents measured{ Ampere{ sampled.a.Value() - shift.a }, Ampere{ sampled.b.Value() - shift.b }, Ampere{ sampled.c.Value() - shift.c } };
            const auto a = std::max(previous.a.Value(), latest.a.Value());
            const auto b = std::max(previous.b.Value(), latest.b.Value());
            const auto c = std::max(previous.c.Value(), latest.c.Value());

            if (std::max({ a, b, c }) <= maxReadable)
                return measured;

            if (a >= b && a >= c)
                return PhaseCurrents{ Ampere{ offsetSum - (measured.b.Value() + measured.c.Value()) }, measured.b, measured.c };
            if (b >= c)
                return PhaseCurrents{ measured.a, Ampere{ offsetSum - (measured.a.Value() + measured.c.Value()) }, measured.c };
            return PhaseCurrents{ measured.a, measured.b, Ampere{ offsetSum - (measured.a.Value() + measured.b.Value()) } };
        }

    private:
        uint32_t maxReadable;
        // Written from the event loop, read in the control interrupt.
        float offsetSum{ 0.0f };
        PhaseSampleDelays delays{};
        // Written in the control interrupt; cleared from the event loop once the bridge has stopped.
        ThreePhase shift{};
        PhasePwmDutyCycles previous{};
        PhasePwmDutyCycles latest{};
    };
}
