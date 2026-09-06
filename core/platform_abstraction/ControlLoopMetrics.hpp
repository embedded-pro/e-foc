#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <cstddef>
#include <cstdint>

namespace application
{
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

        void Configure(uint32_t budgetCycles, uint32_t periodCycles);

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

        ALWAYS_INLINE_HOT void RecordReentry()
        {
            reentryCount = Increment(reentryCount);
        }

        void Reset();

        Snapshot Read() const;

    private:
        static constexpr uint32_t saturated = 0xFFFFFFFFu;
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
        uint32_t scaledAverage{ 0 };
        uint32_t overrunCount{ 0 };
        uint32_t deadlineMissCount{ 0 };
        uint32_t reentryCount{ 0 };
    };
}
