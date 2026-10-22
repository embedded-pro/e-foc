#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <algorithm>
#include <cstdint>

namespace foc
{
    // A low-side shunt reads its phase only while that leg's low-side switch conducts around the PWM centre, so a
    // duty above the board's limit hides the phase from the current loop. Lowering all three duties together keeps
    // the line-to-line voltages, and with them the vector the current loop asked for. A vector wider than the limit
    // takes the lowest duty to zero and keeps the middle one within the limit, so two phases stay readable, and the
    // highest may go up to the board's maximum duty: LowSideShuntReadout gives its current from the other two. Only
    // a vector whose middle duty is still above the limit, or whose highest is above the maximum, is scaled down,
    // keeping its direction.
    ALWAYS_INLINE_HOT PhasePwmDutyCycles KeepLowSideShuntsReadable(const PhasePwmDutyCycles& duties, hal::DutyCycle maxReadable, hal::DutyCycle maxDuty)
    {
        const auto limit = maxReadable.Value();
        const auto highest = std::max({ duties.a.Value(), duties.b.Value(), duties.c.Value() });

        if (highest <= limit)
            return duties;

        const auto lowest = std::min({ duties.a.Value(), duties.b.Value(), duties.c.Value() });
        const auto middle = duties.a.Value() + duties.b.Value() + duties.c.Value() - highest - lowest;
        const auto shift = std::min(highest - limit, lowest);
        const auto within = [](uint32_t bound, uint32_t value)
        {
            return value > bound ? static_cast<float>(bound) / static_cast<float>(value) : 1.0f;
        };
        const auto scale = std::min(within(limit, middle - shift), within(maxDuty.Value(), highest - shift));

        const auto keep = [shift, scale](hal::DutyCycle duty)
        {
            return hal::DutyCycle{ static_cast<uint32_t>(static_cast<float>(duty.Value() - shift) * scale + 0.5f) };
        };

        return PhasePwmDutyCycles{ keep(duties.a), keep(duties.b), keep(duties.c) };
    }

    // The samples of a PWM centre fall in the low-side pulse that the period ending there and the one starting there
    // share, so a phase is unreadable if either period gave it a duty above the limit. KeepLowSideShuntsReadable
    // leaves at most the highest phase there, and the winding's currents sum to zero.
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
