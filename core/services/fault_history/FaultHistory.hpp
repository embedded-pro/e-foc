#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/fault_history/FaultLog.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>
#include <optional>

namespace services
{
    class FaultHistory
        : public FaultLog
    {
    public:
        static constexpr infra::Duration storeInterval{ std::chrono::seconds(1) };
        static constexpr infra::Duration busyRetry{ std::chrono::milliseconds(10) };
        static constexpr uint16_t maxPowerUps{ 32767 };

        struct Conditions
        {
            uint8_t mode;
            foc::Volts busVoltage;
            foc::Ampere peakCurrent;
        };

        // Brackets each non-volatile operation, so the control mode starts no NVM work of its own meanwhile.
        FaultHistory(NonVolatileMemory& nvm, const infra::Function<void()>& holdWork, const infra::Function<void()>& releaseWork);

        void Start();
        void Record(uint8_t code, const Conditions& conditions);
        std::optional<FaultRecord> OfAge(std::size_t age) const override;
        void Clear(const infra::Function<void(bool stored)>& onDone) override;

    private:
        void Load();
        void Loaded(NvmStatus status);
        void Insert(const FaultRecord& record);
        void ScheduleStore();
        void Store();
        void Stored(NvmStatus status);

        NonVolatileMemory& nvm;
        infra::Function<void()> holdWork;
        infra::Function<void()> releaseWork;
        const infra::TimePoint powerUpTime;
        FaultHistoryData history;
        infra::BoundedVector<FaultRecord>::WithMaxSize<FaultHistoryData::capacity> early;
        bool loaded{ false };
        bool storing{ false };
        bool dirty{ false };
        std::optional<infra::TimePoint> lastStore;
        infra::TimerSingleShot timer;
        infra::AutoResetFunction<void(bool)> onClearRequested;
        infra::AutoResetFunction<void(bool)> onClearStoring;
    };
}
