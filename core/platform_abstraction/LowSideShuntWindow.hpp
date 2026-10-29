#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
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

        ALWAYS_INLINE_HOT PhaseCurrents Read(const PhaseCurrents& measured) const
        {
            const auto a = std::max(previous.a.Value(), latest.a.Value());
            const auto b = std::max(previous.b.Value(), latest.b.Value());
            const auto c = std::max(previous.c.Value(), latest.c.Value());

            if (std::max({ a, b, c }) <= maxReadable)
                return measured;

            if (a >= b && a >= c)
                return PhaseCurrents{ Ampere{ -(measured.b.Value() + measured.c.Value()) }, measured.b, measured.c };
            if (b >= c)
                return PhaseCurrents{ measured.a, Ampere{ -(measured.a.Value() + measured.c.Value()) }, measured.c };
            return PhaseCurrents{ measured.a, measured.b, Ampere{ -(measured.a.Value() + measured.b.Value()) } };
        }

    private:
        uint32_t maxReadable;
        PhasePwmDutyCycles previous{};
        PhasePwmDutyCycles latest{};
    };
}
