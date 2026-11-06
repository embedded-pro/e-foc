#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/NvmActivity.hpp"
#include "core/state_machine/Recalibration.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>
#include <optional>

namespace state_machine
{
    // CRC-32 of the commissioned inductance, inertia and viscous friction a learned model belongs to.
    uint32_t CommissioningCheck(const MotorModel& commissioned);

    class RecalibrationStore
    {
    public:
        static constexpr infra::Duration storeInterval{ std::chrono::minutes(15) };
        static constexpr infra::Duration busyRetry{ std::chrono::milliseconds(10) };

        RecalibrationStore(services::NonVolatileMemory& nvm, application::NvmActivity& activity);

        void Start(const infra::Function<void()>& onLoaded);
        // Empty unless the record was learned from these commissioned inductance, inertia and friction.
        StoredModel Restored(const MotorModel& commissioned) const;
        // Stores now when no store happened in the last 15 minutes of uptime, else at the first StoreIfDue after them.
        void Store(const StoredModel& learned, const MotorModel& commissioned);
        void StoreIfDue();
        void Reset(const infra::Function<void(bool stored)>& onDone);
        uint16_t Stores() const;

    private:
        void Load();
        void Loaded(services::NvmStatus status);
        void Proceed();
        void Write(const services::LearnedModelData& model, bool reset);
        void Written(services::NvmStatus status);
        bool Due() const;

        services::NonVolatileMemory& nvm;
        application::NvmActivity& activity;
        infra::AutoResetFunction<void()> onLoaded;
        infra::AutoResetFunction<void(bool)> onResetRequested;
        infra::AutoResetFunction<void(bool)> onResetWriting;
        services::LearnedModelData read;
        services::LearnedModelData restored;
        services::LearnedModelData writing;
        std::optional<services::LearnedModelData> waiting;
        std::optional<services::LearnedModelData> last;
        std::optional<infra::TimePoint> lastStore;
        bool loaded{ false };
        bool storing{ false };
        bool resetSinceStart{ false };
        uint16_t stores{ 0 };
        infra::TimerSingleShot retry;
    };
}
