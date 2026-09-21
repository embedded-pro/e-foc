#include "core/state_machine/FaultController.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace application
{
    void FaultController::Register(
        state_machine::FaultNotifier& notifier,
        const infra::Function<void(state_machine::FaultCode)>& onImmediate,
        const infra::Function<void(state_machine::FaultCode)>& onDeferred)
    {
        registeredNotifier = &notifier;
        notifier.Register(onImmediate, onDeferred);
    }

    void FaultController::Unregister()
    {
        if (registeredNotifier != nullptr)
        {
            registeredNotifier->Unregister();
            registeredNotifier = nullptr;
        }

        conditionPollTimer.reset();
    }

    void FaultController::LatchFromInterrupt(state_machine::FaultCode code)
    {
        pendingCode = code;
        faultPending = true;
        faultLatched = true;
    }

    std::optional<state_machine::FaultCode> FaultController::TakePendingFault()
    {
        if (!faultPending)
            return std::nullopt;

        const auto code = pendingCode;
        faultPending = false;
        return code;
    }

    void FaultController::EnterFault()
    {
        faultLatched = true;
        faultRecorded = true;

        // Polled rather than sampled per command, so that a condition chattering between two operator
        // commands is still observed; the timer is cancelled by its own destructor if this object dies
        conditionPollTimer.emplace(conditionPollInterval, [this]()
            {
                SampleCondition();
            });
    }

    void FaultController::SampleCondition()
    {
        const auto sampled = registeredNotifier != nullptr ? registeredNotifier->ConditionState() : state_machine::FaultConditionState::unknown;

        if (sampled == state_machine::FaultConditionState::asserted)
            conditionClearSince = std::nullopt;
        else if (sampled == state_machine::FaultConditionState::clear && lastCondition != state_machine::FaultConditionState::clear)
            conditionClearSince = infra::Now();

        lastCondition = sampled;
    }

    state_machine::FaultConditionState FaultController::ConditionState() const
    {
        return lastCondition;
    }

    ClearRefusal FaultController::EvaluateClear() const
    {
        if (lastCondition == state_machine::FaultConditionState::asserted)
            return ClearRefusal::conditionAsserted;

        if (lastCondition == state_machine::FaultConditionState::clear && (!conditionClearSince.has_value() || infra::Now() - *conditionClearSince < conditionDwell))
            return ClearRefusal::dwellNotElapsed;

        if (consecutiveFaultClears >= maxConsecutiveFaultClears)
            return ClearRefusal::retryLimitReached;

        return ClearRefusal::none;
    }

    bool FaultController::CanClear() const
    {
        return EvaluateClear() == ClearRefusal::none;
    }

    void FaultController::Clear()
    {
        // The budget is the only invariant asserted here: the condition is re-evaluated where the transition
        // commits, and a condition that re-asserts in that window must refuse the clear, not reset the board
        really_assert(consecutiveFaultClears < maxConsecutiveFaultClears);

        ++consecutiveFaultClears;
        faultLatched = false;
        faultRecorded = false;
        conditionPollTimer.reset();
        conditionClearSince = std::nullopt;
        lastCondition = state_machine::FaultConditionState::unknown;
    }

    bool FaultController::TryClear()
    {
        if (!CanClear())
            return false;

        Clear();
        return true;
    }

    void FaultController::ResetClearCount()
    {
        consecutiveFaultClears = 0;
    }

    bool FaultController::IsLatched() const
    {
        return faultLatched;
    }

    bool FaultController::IsRecorded() const
    {
        return faultRecorded;
    }

    bool FaultController::IsPending() const
    {
        return faultPending;
    }
}
