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
    }

    bool FaultController::CanClear() const
    {
        return consecutiveFaultClears < maxConsecutiveFaultClears;
    }

    void FaultController::Clear()
    {
        really_assert(CanClear());

        ++consecutiveFaultClears;
        faultLatched = false;
        faultRecorded = false;
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
