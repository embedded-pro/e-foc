#pragma once

#include "core/state_machine/FaultNotifier.hpp"
#include <gmock/gmock.h>

namespace state_machine
{
    class FaultNotifierMock
        : public FaultNotifier
    {
    public:
        MOCK_METHOD(void, Register, (const infra::Function<void(FaultCode)>& onImmediate, const infra::Function<void(FaultCode)>& onDeferred), (override));
        MOCK_METHOD(void, Unregister, (), (override));

        void StoreHandler(const infra::Function<void(FaultCode)>& immediate, const infra::Function<void(FaultCode)>& deferred)
        {
            storedImmediate = immediate;
            storedDeferred = deferred;
        }

        void TriggerFault(FaultCode code)
        {
            TriggerFaultWithoutDispatch(code);

            if (storedDeferred)
                storedDeferred(code);
        }

        void TriggerFaultWithoutDispatch(FaultCode code)
        {
            if (storedImmediate)
                storedImmediate(code);
        }

        void ReleaseHandler()
        {
            storedImmediate = nullptr;
            storedDeferred = nullptr;
        }

    private:
        infra::Function<void(FaultCode)> storedImmediate;
        infra::Function<void(FaultCode)> storedDeferred;
    };
}
