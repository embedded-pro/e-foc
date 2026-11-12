#pragma once

#include "can-lite/core/MonitoredCan.hpp"
#include "infra/util/ExecutionTimeStatistics.hpp"

namespace application
{
    class PlatformDiagnostics
    {
    public:
        explicit PlatformDiagnostics(infra::ExecutionTimeStatistics& metrics);

        void AttachCanBus(services::MonitoredCan& bus);

        infra::ExecutionTimeStatistics::Snapshot ControlLoopStatistics() const;
        const services::CanBusErrorCounters& CanStatistics() const;
        void Reset();

    private:
        infra::ExecutionTimeStatistics& metrics;
        services::MonitoredCan* canBus{ nullptr };
    };
}
