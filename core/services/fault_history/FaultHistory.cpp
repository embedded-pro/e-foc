#include "core/services/fault_history/FaultHistory.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace services
{
    namespace
    {
        int16_t Hundredths(float value)
        {
            constexpr float limit{ static_cast<float>(std::numeric_limits<int16_t>::max()) };
            return static_cast<int16_t>(math::Round(std::clamp(value * 100.0f, -limit, limit)));
        }

        bool Older(const FaultRecord& left, const FaultRecord& right)
        {
            return left.powerUp != right.powerUp ? left.powerUp < right.powerUp : left.uptimeMs < right.uptimeMs;
        }
    }

    FaultHistory::FaultHistory(NonVolatileMemory& nvm, const infra::Function<void()>& holdWork, const infra::Function<void()>& releaseWork)
        : nvm{ nvm }
        , holdWork{ holdWork }
        , releaseWork{ releaseWork }
    {}

    void FaultHistory::Start()
    {
        Load();
    }

    void FaultHistory::Load()
    {
        holdWork();
        nvm.LoadFaultHistory(history, [this](NvmStatus status)
            {
                releaseWork();
                Loaded(status);
            });
    }

    void FaultHistory::Loaded(NvmStatus status)
    {
        if (status == NvmStatus::Busy)
        {
            timer.Start(busyRetry, [this]()
                {
                    Load();
                });
            return;
        }

        if (status != NvmStatus::Ok)
            history = FaultHistoryData{};

        history.powerUps = static_cast<uint16_t>(std::min<uint32_t>(history.powerUps + 1u, maxPowerUps));
        loaded = true;

        for (auto& record : early)
        {
            record.powerUp = history.powerUps;
            Insert(record);
        }
        early.clear();

        dirty = true;
        ScheduleStore();
        ReleaseWaiting();
    }

    void FaultHistory::WhenLoaded(const LoadedAction& action)
    {
        if (loaded)
        {
            action();
            return;
        }

        if (waiting.full())
        {
            const auto oldest = waiting.front();
            waiting.erase(waiting.begin());
            oldest();
        }

        waiting.push_back(action);
    }

    void FaultHistory::ReleaseWaiting()
    {
        while (!waiting.empty())
        {
            const auto action = waiting.front();
            waiting.erase(waiting.begin());
            action();
        }
    }

    void FaultHistory::Record(uint8_t code, const Conditions& conditions)
    {
        const auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(infra::Now() - powerUpTime);
        const FaultRecord record{ code, conditions.mode, history.powerUps, static_cast<uint32_t>(uptime.count()),
            Hundredths(conditions.busVoltage.Value()), Hundredths(conditions.peakCurrent.Value()) };

        if (!loaded)
        {
            if (early.full())
                early.erase(early.begin());
            early.push_back(record);
            return;
        }

        Insert(record);
        dirty = true;
        ScheduleStore();
    }

    void FaultHistory::Insert(const FaultRecord& record)
    {
        auto& records = history.records;
        auto slot = std::ranges::find_if(records, [](const std::optional<FaultRecord>& candidate)
            {
                return !candidate.has_value();
            });

        if (slot == records.end())
            slot = std::ranges::min_element(records, [](const std::optional<FaultRecord>& left, const std::optional<FaultRecord>& right)
                {
                    return Older(*left, *right);
                });

        *slot = record;
    }

    std::optional<FaultRecord> FaultHistory::OfAge(std::size_t age) const
    {
        const auto& records = history.records;
        infra::BoundedVector<std::size_t>::WithMaxSize<FaultHistoryData::capacity> held;
        for (std::size_t slot = 0; slot != records.size(); ++slot)
            if (records[slot].has_value())
                held.push_back(slot);

        if (age >= held.size())
            return std::nullopt;

        std::ranges::sort(held, [&records](std::size_t left, std::size_t right)
            {
                if (Older(*records[right], *records[left]))
                    return true;
                return !Older(*records[left], *records[right]) && left > right;
            });
        return records[held[age]];
    }

    void FaultHistory::Clear(const infra::Function<void(bool stored)>& onDone)
    {
        history.records = {};
        onClearRequested = onDone;
        dirty = true;
        ScheduleStore();
    }

    void FaultHistory::ScheduleStore()
    {
        if (!loaded || storing || timer.Armed())
            return;

        const auto now = infra::Now();
        const auto due = lastStore.has_value() ? *lastStore + storeInterval : now;
        if (due <= now)
            Store();
        else
            timer.Start(due - now, [this]()
                {
                    Store();
                });
    }

    void FaultHistory::Store()
    {
        storing = true;
        dirty = false;
        if (onClearRequested != nullptr)
            onClearStoring = onClearRequested.Clone();
        onClearRequested = nullptr;

        holdWork();
        nvm.SaveFaultHistory(history, [this](NvmStatus status)
            {
                releaseWork();
                Stored(status);
            });
    }

    void FaultHistory::Stored(NvmStatus status)
    {
        storing = false;

        if (status == NvmStatus::Busy)
        {
            dirty = true;
            if (onClearStoring != nullptr)
                onClearRequested = onClearStoring.Clone();
            onClearStoring = nullptr;
            timer.Start(busyRetry, [this]()
                {
                    Store();
                });
            return;
        }

        lastStore = infra::Now();
        if (status != NvmStatus::Ok)
            dirty = true;
        if (onClearStoring != nullptr)
            onClearStoring(status == NvmStatus::Ok);

        if (dirty)
            ScheduleStore();
    }
}
