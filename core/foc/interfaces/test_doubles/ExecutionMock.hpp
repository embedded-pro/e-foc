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

    class ControllableMock
        : public Controllable
    {
    public:
        MOCK_METHOD(void, Start, (), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };

    class PhaseCurrentsObservableMock
        : public PhaseCurrentsObservable
    {
    public:
        MOCK_METHOD(void, RegisterPhaseCurrentsObserver, (const infra::Function<void(const PhaseCurrents& currentPhases)>& observer), (override));
        MOCK_METHOD(void, UnregisterPhaseCurrentsObserver, (), (override));

        void StoreObserver(const infra::Function<void(const PhaseCurrents& currentPhases)>& observer)
        {
            storedObserver = observer;
        }

        void ReleaseObserver()
        {
            storedObserver = nullptr;
        }

        void Publish(const PhaseCurrents& currentPhases)
        {
            if (storedObserver != nullptr)
                storedObserver(currentPhases);
        }

        bool HasObserver() const
        {
            return storedObserver != nullptr;
        }

    private:
        infra::Function<void(const PhaseCurrents& currentPhases)> storedObserver;
    };
}
