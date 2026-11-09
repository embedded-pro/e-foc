#include "core/platform_abstraction/PlatformDiagnostics.hpp"

namespace application
{
    namespace
    {
        constexpr services::CanBusErrorCounters noBus{};
    }

    PlatformDiagnostics::PlatformDiagnostics(infra::ExecutionTimeStatistics& metrics)
        : metrics(metrics)
    {}

    void PlatformDiagnostics::AttachCanBus(services::MonitoredCan& bus)
    {
        canBus = &bus;
    }

    infra::ExecutionTimeStatistics::Snapshot PlatformDiagnostics::ControlLoopStatistics() const
    {
        return metrics.Read();
    }

    const services::CanBusErrorCounters& PlatformDiagnostics::CanStatistics() const
    {
        return canBus == nullptr ? noBus : canBus->ErrorStatistics();
    }

    void PlatformDiagnostics::Reset()
    {
        metrics.Reset();

        if (canBus != nullptr)
            canBus->ResetErrorStatistics();
    }
}
