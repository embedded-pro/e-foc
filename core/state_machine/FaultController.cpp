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

    void FaultController::EnterFault()
    {
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
}
