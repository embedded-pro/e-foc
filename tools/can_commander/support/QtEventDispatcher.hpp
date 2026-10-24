#pragma once

#include "infra/event/EventDispatcher.hpp"
#include <QObject>
#include <atomic>

namespace tool
{
    // Runs EmIL's scheduled actions on the Qt GUI thread. Schedule may be called from any thread
    // (the generic timer service schedules from its own); each burst costs one queued Qt event
    // instead of a polling timer.
    class QtEventDispatcherWorker
        : public infra::EventDispatcherWorkerImpl
    {
    public:
        template<std::size_t StorageSize, class T = QtEventDispatcherWorker>
        using WithSize = infra::WithStorage<T, std::array<std::pair<infra::Function<void()>, std::atomic<bool>>, StorageSize>>;

        explicit QtEventDispatcherWorker(infra::MemoryRange<std::pair<infra::Function<void()>, std::atomic<bool>>> scheduledActionsStorage);

    protected:
        void RequestExecution() override;

    private:
        QObject context;
        std::atomic<bool> executionRequested{ false };
    };

    using QtEventDispatcher = infra::EventDispatcherConnector<QtEventDispatcherWorker>;
}
