#pragma once

#include "core/platform_abstraction/interfaces/Drivers.hpp"

namespace application
{
    class WatchdogPlaceholder
        : public drivers::Watchdog
    {
    public:
        // Implementation of drivers::Watchdog
        void Enable(std::chrono::microseconds, const infra::Function<void()>&) override
        {}

        void Feed() override
        {}

        bool IsEnabled() const override
        {
            return false;
        }

        std::chrono::microseconds Deadline() const override
        {
            return std::chrono::microseconds{ 0 };
        }
    };
}
