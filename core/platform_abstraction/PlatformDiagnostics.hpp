#pragma once

#include "core/platform_abstraction/CanBusAdapter.hpp"
#include "core/platform_abstraction/ControlLoopMetrics.hpp"

namespace application
{
    class PlatformDiagnostics
    {
    public:
        explicit PlatformDiagnostics(ControlLoopMetrics& metrics);

        void AttachCanBus(CanBusAdapter& bus);

        ControlLoopMetrics::Snapshot ControlLoopStatistics() const;
        const CanBusAdapter::ErrorCounters& CanStatistics() const;
        void Reset();

    private:
        ControlLoopMetrics& metrics;
        CanBusAdapter* canBus{ nullptr };
        CanBusAdapter::ErrorCounters noBus;
    };
}
