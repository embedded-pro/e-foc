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
        MOCK_METHOD(void, Unregister, (), (override));

        void StoreHandler(const infra::Function<void()>& handler)
        {
            storedHandler = handler;
        }

        void ClearHandler()
        {
            storedHandler = nullptr;
        }

        void TriggerHandler()
        {
            if (storedHandler)
                storedHandler();
        }

        bool HasHandler() const
        {
            return storedHandler != nullptr;
        }

    private:
        infra::Function<void()> storedHandler;
    };
}
