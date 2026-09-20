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

        void EnterFault();
        bool CanClear() const;
        void Clear();
        bool TryClear();
        void ResetClearCount();

        bool IsLatched() const;

    private:
        state_machine::FaultNotifier* registeredNotifier{ nullptr };
        bool faultLatched{ false };
        uint8_t consecutiveFaultClears{ 0 };
        static constexpr uint8_t maxConsecutiveFaultClears{ 3 };
    };
}
