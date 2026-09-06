#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <cstddef>
#include <cstdint>

namespace application
{
    // Execution statistics for the control interrupt, accumulated in the interrupt itself and read
    // from the command line or a telemetry frame. #255 reports the drive as having no cycle
    // counter, no execution-time statistics, no overrun counter and no deadline-miss detection;
    // this is that layer.
    //
    // No heap, no loops, no division on the update path. Every counter saturates rather than
    // wrapping, so a reader cannot mistake a long run for a healthy one.
    class ControlLoopMetrics
    {
    public:
        struct Snapshot
        {
            uint32_t samples{ 0 };
            uint32_t lastCycles{ 0 };
            uint32_t minimumCycles{ 0 };
            uint32_t maximumCycles{ 0 };
            uint32_t averageCycles{ 0 };
            uint32_t budgetCycles{ 0 };
            uint32_t overruns{ 0 };
            uint32_t deadlineMisses{ 0 };
            uint32_t reentries{ 0 };
        };

        // The budget is the cycle count the interrupt may take; the period is what it must fit
        // inside. Exceeding the first is an overrun - margin gone, but the loop still met its
        // deadline. Exceeding the second is a deadline miss: the next conversion was already due.
        void Configure(uint32_t budgetCycles, uint32_t periodCycles);

        // Records one completed run. `cycles` is the free-running counter's delta across it, so it
        // stays correct across the counter's own wrap.
        //
        // Body lives in the header so it can inline at the one interrupt-context call site; the
        // project builds without LTO, so a .cpp definition costs a call there instead.
        ALWAYS_INLINE_HOT void Record(uint32_t cycles)
        {
            last = cycles;
            samples = Increment(samples);

            if (cycles < minimum)
                minimum = cycles;
            if (cycles > maximum)
                maximum = cycles;

            scaledAverage = samples == 1
                                ? cycles << averagingShift
                                : scaledAverage + cycles - (scaledAverage >> averagingShift);

            if (budget != 0 && cycles > budget)
                overrunCount = Increment(overrunCount);
            if (period != 0 && cycles > period)
                deadlineMissCount = Increment(deadlineMissCount);
        }

        // Records an interrupt that arrived while the previous one had not returned. Distinct from
        // a deadline miss, which is measured after the fact from the duration.
        ALWAYS_INLINE_HOT void RecordReentry()
        {
            reentryCount = Increment(reentryCount);
        }

        void Reset();

        Snapshot Read() const;

    private:
        static constexpr uint32_t saturated = 0xFFFFFFFFu;
        // 1/16, as a shift: new = old + (sample - old) / 16. A first-order filter over roughly the
        // last sixteen runs, which is enough to show a trend without holding a history.
        static constexpr uint32_t averagingShift = 4;

        ALWAYS_INLINE_HOT static uint32_t Increment(uint32_t counter)
        {
            return counter == saturated ? counter : counter + 1;
        }

        uint32_t budget{ 0 };
        uint32_t period{ 0 };
        uint32_t samples{ 0 };
        uint32_t last{ 0 };
        uint32_t minimum{ saturated };
        uint32_t maximum{ 0 };
        // Held scaled by 2^averagingShift so the filter needs no division and loses no resolution.
        uint32_t scaledAverage{ 0 };
        uint32_t overrunCount{ 0 };
        uint32_t deadlineMissCount{ 0 };
        uint32_t reentryCount{ 0 };
    };
}
