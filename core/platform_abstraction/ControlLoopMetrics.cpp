#include "core/platform_abstraction/ControlLoopMetrics.hpp"

namespace application
{
    void ControlLoopMetrics::Configure(uint32_t budgetCycles, uint32_t periodCycles)
    {
        budget = budgetCycles == 0 ? saturated : budgetCycles;
        period = periodCycles == 0 ? saturated : periodCycles;
        Reset();
    }

    void ControlLoopMetrics::Reset()
    {
        samples = 0;
        last = 0;
        minimum = saturated;
        maximum = 0;
        scaledAverage = 0;
        overrunCount = 0;
        deadlineMissCount = 0;
        reentryCount = 0;
    }

    ControlLoopMetrics::Snapshot ControlLoopMetrics::Read() const
    {
        return Snapshot{
            samples,
            last,
            samples == 0 ? 0u : minimum,
            maximum,
            scaledAverage >> averagingShift,
            budget == saturated ? 0u : budget,
            overrunCount,
            deadlineMissCount,
            reentryCount
        };
    }
}
