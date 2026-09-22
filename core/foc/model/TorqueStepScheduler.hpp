#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <atomic>
#include <cstdint>
#include <optional>

namespace foc
{
    struct TorqueStepSchedule
    {
        float torqueNm{ 0.0f };
        uint32_t delayTicks{ 0 };
    };

    // Begin only raises a flag; the control tick owns the arming so the fire tick is exact.
    class TorqueStepScheduler
    {
    public:
        void Configure(const TorqueStepSchedule& schedule);
        bool IsEnabled() const;

        void Begin();

        OPTIMIZE_FOR_SPEED std::optional<float> Fire(uint32_t tick);

    private:
        TorqueStepSchedule schedule{};
        std::atomic<bool> pendingBegin{ false };
        bool armed{ false };
        uint32_t beginTick{ 0 };
    };

    inline void TorqueStepScheduler::Configure(const TorqueStepSchedule& schedule)
    {
        this->schedule = schedule;
    }

    inline bool TorqueStepScheduler::IsEnabled() const
    {
        return schedule.torqueNm != 0.0f;
    }

    inline void TorqueStepScheduler::Begin()
    {
        if (IsEnabled())
            pendingBegin.store(true, std::memory_order_release);
    }

    inline std::optional<float> TorqueStepScheduler::Fire(uint32_t tick)
    {
        if (pendingBegin.exchange(false, std::memory_order_acq_rel))
        {
            beginTick = tick;
            armed = true;
        }

        if (!armed || tick - beginTick < schedule.delayTicks)
            return std::nullopt;

        armed = false;
        return schedule.torqueNm;
    }
}
