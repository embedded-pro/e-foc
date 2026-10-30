#include "core/platform_abstraction/PlatformDiagnostics.hpp"

namespace application
{
    namespace
    {
        constexpr CanBusAdapter::ErrorCounters noBus{};
    }

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

    uint32_t PlatformDiagnostics::ControlLoopCompletions() const
    {
        return metrics.Completions();
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
