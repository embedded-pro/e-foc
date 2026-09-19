#pragma once

#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/timer/Timer.hpp"
#include <atomic>

namespace application
{
    class SoftwareWatchdog
        : public drivers::Watchdog
    {
    public:
        // Implementation of drivers::Watchdog
        void Enable(std::chrono::microseconds deadline, const infra::Function<void()>& onDeadlineMissed) override;
        void Feed() override;
        bool IsEnabled() const override;
        std::chrono::microseconds Deadline() const override;

        bool HasExpired() const;

    private:
        void Check();

        std::chrono::microseconds deadline{ 0 };
        infra::Function<void()> onDeadlineMissed;
        infra::TimerRepeating checkTimer;
        std::atomic<bool> fed{ false };
        bool enabled{ false };
        bool expired{ false };
    };
}
