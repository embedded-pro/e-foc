#pragma once

#include "core/state_machine/FaultNotifier.hpp"
#include "infra/util/Function.hpp"

namespace application
{
    class FaultController
    {
    public:
        void Register(
            state_machine::FaultNotifier& notifier,
            const infra::Function<void(state_machine::FaultCode)>& onImmediate,
            const infra::Function<void(state_machine::FaultCode)>& onDeferred);

        void Unregister();

        void LatchFromInterrupt(state_machine::FaultCode code);
        bool TakePendingFault();

        void EnterFault();
        bool CanClear() const;
        void Clear();
        bool TryClear();
        void ResetClearCount();

        bool IsLatched() const;
        bool IsRecorded() const;
        state_machine::FaultCode PendingCode() const;

    private:
        state_machine::FaultNotifier* registeredNotifier{ nullptr };
        // Set in the faulting context, so it is true from the interrupt onwards; faultRecorded only once the
        // transition it owes has been taken. The two differ exactly in the window this class exists to cover.
        volatile bool faultLatched{ false };
        volatile bool faultPending{ false };
        volatile state_machine::FaultCode pendingCode{ state_machine::FaultCode::none };
        bool faultRecorded{ false };
        uint8_t consecutiveFaultClears{ 0 };
        static constexpr uint8_t maxConsecutiveFaultClears{ 3 };
    };
}
