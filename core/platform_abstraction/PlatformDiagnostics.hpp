#pragma once

#include "can-lite/core/MonitoredCan.hpp"
#include "core/platform_abstraction/ControlLoopMetrics.hpp"

namespace application
{
    class PlatformDiagnostics
    {
    public:
        explicit PlatformDiagnostics(ControlLoopMetrics& metrics);

        void AttachCanBus(services::MonitoredCan& bus);

        ControlLoopMetrics::Snapshot ControlLoopStatistics() const;
        const services::CanBusErrorCounters& CanStatistics() const;
        void Reset();

    private:
        ControlLoopMetrics& metrics;
        services::MonitoredCan* canBus{ nullptr };
    };
}
