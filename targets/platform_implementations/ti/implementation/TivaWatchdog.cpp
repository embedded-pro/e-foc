#include "targets/platform_implementations/ti/implementation/TivaWatchdog.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace application
{
    void TivaWatchdog::Enable(std::chrono::microseconds deadline, const infra::Function<void()>& onDeadlineMissed)
    {
        really_assert(!hardwareWatchdog.has_value());

        progressWatchdog.Enable(deadline, onDeadlineMissed);

        hal::tiva::WatchDog::Config config;
        config.expirationTimeout = std::chrono::duration_cast<infra::Duration>(deadline);
        config.resetOnMissedInterrupt = true;

        hardwareWatchdog.emplace(watchDogIndex, onDeadlineMissed, config);
    }

    void TivaWatchdog::Feed()
    {
        progressWatchdog.Feed();
    }

    bool TivaWatchdog::IsEnabled() const
    {
        return progressWatchdog.IsEnabled();
    }

    std::chrono::microseconds TivaWatchdog::Deadline() const
    {
        return progressWatchdog.Deadline();
    }
}
