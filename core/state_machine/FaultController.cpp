#include "core/state_machine/FaultController.hpp"

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

    bool FaultController::TakePendingFault()
    {
        if (!faultPending)
            return false;

        faultPending = false;
        return true;
    }

    void FaultController::EnterFault()
    {
        faultPending = false;
        faultLatched = true;
    }

    bool FaultController::TryClear()
    {
        if (consecutiveFaultClears >= maxConsecutiveFaultClears)
            return false;

        ++consecutiveFaultClears;
        faultLatched = false;
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

    state_machine::FaultCode FaultController::PendingCode() const
    {
        return pendingCode;
    }
}
