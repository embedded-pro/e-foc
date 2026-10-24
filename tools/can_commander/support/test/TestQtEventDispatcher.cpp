#include "tools/can_commander/support/QtEventDispatcher.hpp"
#include "gtest/gtest.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace
{
    class TestQtEventDispatcher
        : public testing::Test
    {
    public:
        static void SetUpTestSuite()
        {
            static int argc{ 1 };
            static char name[]{ "support_test" };
            static char* argv[]{ name, nullptr };

            if (QCoreApplication::instance() == nullptr)
                application = std::make_unique<QCoreApplication>(argc, argv);
        }

        static void TearDownTestSuite()
        {
            application.reset();
        }

        void RunUntil(const std::function<bool()>& done)
        {
            QEventLoop loop;
            QTimer poll;
            QObject::connect(&poll, &QTimer::timeout, &loop, [&loop, &done]()
                {
                    if (done())
                        loop.quit();
                });
            QTimer::singleShot(5000, &loop, &QEventLoop::quit);
            poll.start(1);
            loop.exec();
        }

        static inline std::unique_ptr<QCoreApplication> application;
        tool::QtEventDispatcher::WithSize<16> dispatcher;
    };
}

TEST_F(TestQtEventDispatcher, runs_actions_from_the_qt_event_loop)
{
    int executed{ 0 };
    infra::EventDispatcher::Instance().Schedule([&executed]()
        {
            ++executed;
        });

    EXPECT_EQ(0, executed);
    RunUntil([&executed]()
        {
            return executed == 1;
        });
    EXPECT_EQ(1, executed);
}

TEST_F(TestQtEventDispatcher, runs_actions_scheduled_from_another_thread_on_the_gui_thread)
{
    std::atomic<QThread*> executedOn{ nullptr };
    std::thread worker{ [&executedOn]()
        {
            infra::EventDispatcher::Instance().Schedule([&executedOn]()
                {
                    executedOn = QThread::currentThread();
                });
        } };
    worker.join();

    RunUntil([&executedOn]()
        {
            return executedOn != nullptr;
        });
    EXPECT_EQ(QCoreApplication::instance()->thread(), executedOn.load());
}

TEST_F(TestQtEventDispatcher, runs_actions_scheduled_while_executing)
{
    std::vector<int> order;
    infra::EventDispatcher::Instance().Schedule([&order]()
        {
            order.push_back(1);
            infra::EventDispatcher::Instance().Schedule([&order]()
                {
                    order.push_back(2);
                });
        });

    RunUntil([&order]()
        {
            return order.size() == 2;
        });
    EXPECT_EQ((std::vector<int>{ 1, 2 }), order);
}
