#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <atomic>
#include <cstdint>

namespace foc
{
    // Begin only raises a flag; the control tick owns the arming so the fire tick is exact.
    class EncoderFreezeScheduler
    {
    public:
        void Configure(uint32_t delayTicks);
        bool IsEnabled() const;

        void Begin();

        OPTIMIZE_FOR_SPEED bool Fire(uint32_t tick);

    private:
        uint32_t delayTicks{ 0 };
        std::atomic<bool> pendingBegin{ false };
        bool armed{ false };
        uint32_t beginTick{ 0 };
    };

    inline void EncoderFreezeScheduler::Configure(uint32_t delayTicks)
    {
        this->delayTicks = delayTicks;
    }

    inline bool EncoderFreezeScheduler::IsEnabled() const
    {
        return delayTicks != 0;
    }

    inline void EncoderFreezeScheduler::Begin()
    {
        if (IsEnabled())
            pendingBegin.store(true, std::memory_order_release);
    }

    inline bool EncoderFreezeScheduler::Fire(uint32_t tick)
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
