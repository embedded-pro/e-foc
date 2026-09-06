#pragma once

#include "core/state_machine/FaultNotifier.hpp"
#include "infra/event/EventDispatcher.hpp"
#include <gmock/gmock.h>

namespace state_machine
{
    class FaultNotifierMock
        : public FaultNotifier
    {
    public:
        MOCK_METHOD(void, Register, (const infra::Function<void(FaultCode)>& onFault), (override));
        MOCK_METHOD(void, Unregister, (), (override));

        void StoreHandler(const infra::Function<void(FaultCode)>& handler)
        {
            storedHandler = handler;
        }

        // The platform notifier's primary path runs in the PWM fault interrupt and hands the
        // transition to the dispatcher, so delivering a fault to a state machine means running
        // the dispatcher too. TriggerFaultWithoutDispatch stops short of that, for the tests
        // that assert on what does and does not happen inside the interrupt itself.
        void TriggerFault(FaultCode code)
        {
            TriggerFaultWithoutDispatch(code);

            auto& dispatcher = infra::EventDispatcher::Instance();
            while (!dispatcher.IsIdle())
                dispatcher.ExecuteFirstAction();
        }

        void TriggerFaultWithoutDispatch(FaultCode code)
        {
            if (storedHandler)
                storedHandler(code);
        }

        void ReleaseHandler()
        {
            storedHandler = nullptr;
        }

    private:
        infra::Function<void(FaultCode)> storedHandler;
    };
}
