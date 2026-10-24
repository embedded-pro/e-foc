#pragma once

#include "infra/event/EventDispatcher.hpp"
#include <QObject>
#include <atomic>

namespace tool
{
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
