#include "core/platform_abstraction/PlatformDiagnostics.hpp"

namespace application
{
    PlatformDiagnostics::PlatformDiagnostics(ControlLoopMetrics& metrics)
        : metrics(metrics)
    {}

    void PlatformDiagnostics::AttachCanBus(CanBusAdapter& bus)
    {
        canBus = &bus;
    }

    ControlLoopMetrics::Snapshot PlatformDiagnostics::ControlLoopStatistics() const
    {
        return metrics.Read();
    }

    const CanBusAdapter::ErrorCounters& PlatformDiagnostics::CanStatistics() const
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
