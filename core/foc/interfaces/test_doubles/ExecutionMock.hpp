#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include <gmock/gmock.h>

namespace foc
{
    class LowPriorityInterruptMock
        : public LowPriorityInterrupt
    {
    public:
        MOCK_METHOD(void, Trigger, (), (override));
        MOCK_METHOD(void, Register, (const infra::Function<void()>& handler), (override));

        void StoreHandler(const infra::Function<void()>& handler)
        {
            storedHandler = handler;
        }

        void TriggerHandler()
        {
            if (storedHandler)
                storedHandler();
        }

    private:
        infra::Function<void()> storedHandler;
    };
}
