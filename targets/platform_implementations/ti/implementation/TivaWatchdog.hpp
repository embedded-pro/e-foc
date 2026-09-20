#pragma once

#include "core/platform_abstraction/SoftwareWatchdog.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "hal_tiva/tiva/WatchDog.hpp"
#include <optional>

namespace application
{
    class TivaWatchdog
        : public drivers::Watchdog
    {
    public:
        // Implementation of drivers::Watchdog
        void Enable(std::chrono::microseconds deadline, const infra::Function<void()>& onDeadlineMissed) override;
        void Feed() override;
        bool IsEnabled() const override;
        std::chrono::microseconds Deadline() const override;

    private:
        // Runs in the watchdog interrupt, with the event loop already proven stalled
        static void CutPowerStageAndReset();

        static constexpr uint8_t watchDogIndex{ 0 };

        SoftwareWatchdog progressWatchdog;
        std::optional<hal::tiva::WatchDog> hardwareWatchdog;
    };
}
