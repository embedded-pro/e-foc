#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/SharedOptional.hpp"
#include "services/network/connection/test_doubles/ConnectionMock.hpp"
#include "services/network/connection/test_doubles/ConnectionStub.hpp"
#include "tools/hardware_bridge/client/common/TcpClientBase.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <optional>

namespace
{
    class ConcreteTcpClient
        : public tool::TcpClient
    {
    public:
        using tool::TcpClient::TcpClient;

        bool onEstablishedCalled{ false };
        bool onDisconnectedCalled{ false };
        bool lastClearConnectionHandler{ false };

        void OnConnectionEstablished(infra::AutoResetFunction<void(infra::SharedPtr<services::ConnectionObserver>)>&& createdObserver) override
        {
            onEstablishedCalled = true;
            createdObserver = nullptr;
        }

        void OnDisconnected(bool clearConnectionHandler) override
        {
            onDisconnectedCalled = true;
            lastClearConnectionHandler = clearConnectionHandler;
        }
    };

    class TcpClientObserverMock
        : public tool::TcpClientObserver
    {
    public:
        using tool::TcpClientObserver::TcpClientObserver;

        MOCK_METHOD(void, Connected, (), (override));
        MOCK_METHOD(void, Disconnected, (), (override));
    };

    class TestTcpClientBase
        : public testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    protected:
        void CreateAndConnect()
        {
            EXPECT_CALL(connectionFactory, Connect(testing::_))
                .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
                    {
                        capturedClient = &factory;
                    }));

            client.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

            connectionPtr = connectionStub.Emplace();
            capturedClient->ConnectionEstablished([this](infra::SharedPtr<services::ConnectionObserver> observer)
                {
                    connectionPtr->Attach(observer);
                });
            ExecuteAllActions();
        }

        testing::StrictMock<services::ConnectionFactoryMock> connectionFactory;
        infra::SharedOptional<testing::StrictMock<services::ConnectionStub>> connectionStub;
        infra::SharedPtr<services::ConnectionStub> connectionPtr;
        services::ClientConnectionObserverFactory* capturedClient{ nullptr };
        std::optional<ConcreteTcpClient> client;
    };
}

TEST_F(TestTcpClientBase, constructor_calls_factory_connect)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_));

    ConcreteTcpClient c(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);
}

TEST_F(TestTcpClientBase, is_not_connected_initially)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_));

    ConcreteTcpClient c(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

    EXPECT_FALSE(c.IsConnected());
}

TEST_F(TestTcpClientBase, is_connected_after_connection_established)
{
    CreateAndConnect();

    EXPECT_TRUE(client->IsConnected());
}

TEST_F(TestTcpClientBase, on_connection_established_called_after_connect)
{
    CreateAndConnect();

    EXPECT_TRUE(client->onEstablishedCalled);
}

TEST_F(TestTcpClientBase, observers_notified_connected_on_establish)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_))
        .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
            {
                capturedClient = &factory;
            }));

    client.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

    testing::StrictMock<TcpClientObserverMock> observer(*client);

    connectionPtr = connectionStub.Emplace();
    EXPECT_CALL(observer, Connected());
    capturedClient->ConnectionEstablished([this](infra::SharedPtr<services::ConnectionObserver> obs)
        {
            connectionPtr->Attach(obs);
        });
    ExecuteAllActions();
}

TEST_F(TestTcpClientBase, connection_failed_sets_not_connected_and_notifies_disconnected)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_))
        .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
            {
                capturedClient = &factory;
            }));

    client.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

    testing::StrictMock<TcpClientObserverMock> observer(*client);

    EXPECT_CALL(observer, Disconnected());
    capturedClient->ConnectionFailed(services::ClientConnectionObserverFactory::ConnectFailReason::refused);
    ExecuteAllActions();

    EXPECT_FALSE(client->IsConnected());
}

TEST_F(TestTcpClientBase, connection_close_notifies_disconnected_observer)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_))
        .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
            {
                capturedClient = &factory;
            }));

    client.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

    connectionPtr = connectionStub.Emplace();
    testing::StrictMock<TcpClientObserverMock> observer(*client);

    EXPECT_CALL(observer, Connected());
    capturedClient->ConnectionEstablished([this](infra::SharedPtr<services::ConnectionObserver> obs)
        {
            connectionPtr->Attach(obs);
        });
    ExecuteAllActions();

    EXPECT_CALL(observer, Disconnected());
    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    connectionPtr->AbortAndDestroy();
    connectionPtr = nullptr;
    ExecuteAllActions();

    EXPECT_FALSE(client->IsConnected());
}
