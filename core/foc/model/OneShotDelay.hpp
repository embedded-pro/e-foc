#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <atomic>
#include <cstdint>
#include <optional>

namespace foc
{
    // Begin only raises a flag; the control tick owns the arming so the fire tick is exact.
    class OneShotDelay
    {
    public:
        void Configure(std::optional<uint32_t> delayTicks);

        void Begin();

        OPTIMIZE_FOR_SPEED bool Fire(uint32_t tick);

    private:
        bool scheduled{ false };
        uint32_t delayTicks{ 0 };
        std::atomic<bool> pendingBegin{ false };
        bool armed{ false };
        uint32_t beginTick{ 0 };
    };

    inline void OneShotDelay::Configure(std::optional<uint32_t> delayTicks)
    {
        scheduled = delayTicks.has_value();
        this->delayTicks = delayTicks.value_or(0);
    }

    inline void OneShotDelay::Begin()
    {
        if (scheduled)
            pendingBegin.store(true, std::memory_order_release);
    }

    inline bool OneShotDelay::Fire(uint32_t tick)
    {
        if (pendingBegin.exchange(false, std::memory_order_acq_rel))
        {
            beginTick = tick;
            armed = true;
        }

        if (!armed || tick - beginTick < delayTicks)
            return false;

        armed = false;
        return true;
    }
}
