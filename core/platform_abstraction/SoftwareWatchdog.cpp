#include "core/platform_abstraction/SoftwareWatchdog.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace application
{
    void SoftwareWatchdog::Enable(std::chrono::microseconds deadline, const infra::Function<void()>& onDeadlineMissed)
    {
        really_assert(!enabled);
        really_assert(deadline > std::chrono::microseconds::zero());

        this->deadline = deadline;
        this->onDeadlineMissed = onDeadlineMissed;
        enabled = true;
        fed = true;

        checkTimer.Start(std::chrono::duration_cast<infra::Duration>(deadline), [this]()
            {
                Check();
            });
    }

    void SoftwareWatchdog::Feed()
    {
        fed = true;
    }

    bool SoftwareWatchdog::IsEnabled() const
    {
        return enabled;
    }

    std::chrono::microseconds SoftwareWatchdog::Deadline() const
    {
        return deadline;
    }

    bool SoftwareWatchdog::HasExpired() const
    {
        return expired;
    }

    void SoftwareWatchdog::Check()
    {
        if (fed.exchange(false))
            return;

        expired = true;
        checkTimer.Cancel();
        onDeadlineMissed();
    }
}
