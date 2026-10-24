#include "tools/can_commander/support/QtEventDispatcher.hpp"
#include <QMetaObject>

namespace tool
{
    QtEventDispatcherWorker::QtEventDispatcherWorker(infra::MemoryRange<std::pair<infra::Function<void()>, std::atomic<bool>>> scheduledActionsStorage)
        : infra::EventDispatcherWorkerImpl(scheduledActionsStorage)
    {}

    void QtEventDispatcherWorker::RequestExecution()
    {
        if (executionRequested.exchange(true))
            return;

        QMetaObject::invokeMethod(
            &context, [this]()
            {
                executionRequested = false;
                ExecuteAllActions();
            },
            Qt::QueuedConnection);
    }
}
