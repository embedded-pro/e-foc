#pragma once

#include "core/state_machine/FaultNotifier.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
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
        void Clear();
        bool TryClear();
        void ResetClearCount();

        bool IsLatched() const;
        bool IsRecorded() const;
        bool IsPending() const;

    private:
        state_machine::FaultNotifier* registeredNotifier{ nullptr };
        volatile bool faultLatched{ false };
        volatile bool faultPending{ false };
        volatile state_machine::FaultCode pendingCode{ state_machine::FaultCode::none };
        bool faultRecorded{ false };
        state_machine::FaultConditionState lastCondition{ state_machine::FaultConditionState::unknown };
        std::optional<infra::TimePoint> conditionClearSince;
        std::optional<infra::TimerRepeating> conditionPollTimer;
        uint8_t consecutiveFaultClears{ 0 };
        static constexpr uint8_t maxConsecutiveFaultClears{ 3 };
    };
}
