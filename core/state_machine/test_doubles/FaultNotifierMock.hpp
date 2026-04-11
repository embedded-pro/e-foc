#pragma once

#include "core/state_machine/FaultNotifier.hpp"
#include <gmock/gmock.h>

namespace state_machine
{
    class FaultNotifierMock
        : public FaultNotifier
    {
    public:
        MOCK_METHOD(void, Register, (const infra::Function<void(FaultCode)>& onFault), (override));

        void StoreHandler(const infra::Function<void(FaultCode)>& handler)
        {
            storedHandler = handler;
        }

        void TriggerFault(FaultCode code)
        {
            if (storedHandler)
                storedHandler(code);
        }

    private:
        infra::Function<void(FaultCode)> storedHandler;
    };
}
