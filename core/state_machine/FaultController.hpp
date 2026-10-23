#pragma once

#include "core/state_machine/FaultNotifier.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <atomic>
#include <chrono>
#include <optional>

namespace application
{
    enum class ClearRefusal : uint8_t
    {
        none,
        conditionAsserted,
        dwellNotElapsed,
        retryLimitReached
    };

    class FaultController
    {
    public:
        static constexpr std::chrono::milliseconds conditionDwell{ 250 };
        static constexpr std::chrono::milliseconds conditionPollInterval{ 50 };
        static constexpr std::chrono::seconds cleanRunDuration{ 10 };

        void Register(
            state_machine::FaultNotifier& notifier,
            const infra::Function<void(state_machine::FaultCode)>& onImmediate,
            const infra::Function<void(state_machine::FaultCode)>& onDeferred);

        void Unregister();

        void LatchFromInterrupt(state_machine::FaultCode code);
        std::optional<state_machine::FaultCode> TakePendingFault();

        void EnterFault();

        void SampleCondition();
        state_machine::FaultConditionState ConditionState() const;
        ClearRefusal EvaluateClear() const;

        bool CanClear() const;
        bool TryClear();
        void ResetClearCount();

        void StartCleanRun();
        void StopCleanRun();

        bool IsLatched() const;
        bool IsRecorded() const;
        bool IsPending() const;

    private:
        void Clear();

        state_machine::FaultNotifier* registeredNotifier{ nullptr };
        volatile bool faultLatched{ false };
        std::atomic<state_machine::FaultCode> pendingCode{ state_machine::FaultCode::none };
        static_assert(std::atomic<state_machine::FaultCode>::is_always_lock_free, "taken from an interrupt");
        bool faultRecorded{ false };
        state_machine::FaultConditionState lastCondition{ state_machine::FaultConditionState::unknown };
        std::optional<infra::TimePoint> conditionClearSince;
        std::optional<infra::TimerRepeating> conditionPollTimer;
        std::optional<infra::TimerSingleShot> cleanRunTimer;
        uint8_t consecutiveFaultClears{ 0 };
        static constexpr uint8_t maxConsecutiveFaultClears{ 3 };
    };
}
