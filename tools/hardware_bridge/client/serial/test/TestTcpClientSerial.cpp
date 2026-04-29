#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/SharedOptional.hpp"
#include "services/network/connection/test_doubles/ConnectionMock.hpp"
#include "services/network/connection/test_doubles/ConnectionStub.hpp"
#include "tools/hardware_bridge/client/serial/TcpClientSerial.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <optional>

namespace
{
    class TestTcpClientSerial
        : public testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    protected:
        ~TestTcpClientSerial() override
        {
            if (connectionPtr != nullptr)
            {
                EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
                connectionPtr->AbortAndDestroy();
            }
        }

        void CreateAndConnect()
        {
            EXPECT_CALL(connectionFactory, Connect(testing::_))
                .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
                    {
                        capturedClient = &factory;
                    }));

            serial.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

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
        std::optional<tool::TcpClientSerial> serial;
    };
}

TEST_F(TestTcpClientSerial, connects_to_factory)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_));

    tool::TcpClientSerial serial(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);
}

TEST_F(TestTcpClientSerial, destructor_without_connection_does_not_abort)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_));

    serial.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);
    serial.reset();
}

TEST_F(TestTcpClientSerial, send_data_arrives_at_connection)
{
    CreateAndConnect();

    static const uint8_t data[] = { 0x01, 0x02, 0x03 };
    serial->SendData(infra::MakeByteRange(data), []() {});
    ExecuteAllActions();

    ASSERT_EQ(connectionStub->sentData.size(), 3u);
    EXPECT_EQ(connectionStub->sentData[0], 0x01);
    EXPECT_EQ(connectionStub->sentData[1], 0x02);
    EXPECT_EQ(connectionStub->sentData[2], 0x03);
}

TEST_F(TestTcpClientSerial, receive_data_triggers_callback)
{
    CreateAndConnect();

    bool receiveCalled{ false };
    std::vector<uint8_t> received;
    serial->ReceiveData([&receiveCalled, &received](infra::ConstByteRange data)
        {
            receiveCalled = true;
            received.assign(data.begin(), data.end());
        });

    const uint8_t serverData[] = { 0xAA, 0xBB };
    connectionStub->SimulateDataReceived(infra::MakeByteRange(serverData));

    EXPECT_TRUE(receiveCalled);
    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[0], 0xAA);
    EXPECT_EQ(received[1], 0xBB);
}

TEST_F(TestTcpClientSerial, send_data_larger_than_stream_size_is_chunked)
{
    CreateAndConnect();
    connectionStub->maxSendStreamSize = 2;

    static const uint8_t data[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    serial->SendData(infra::MakeByteRange(data), []() {});
    ExecuteAllActions();

    ASSERT_EQ(connectionStub->sentData.size(), 5u);
    for (std::size_t i = 0; i < 5; ++i)
        EXPECT_EQ(connectionStub->sentData[i], data[i]);
}

TEST_F(TestTcpClientSerial, second_send_while_pending_fires_second_callback_immediately)
{
    CreateAndConnect();
    connectionStub->maxSendStreamSize = 2;

    static const uint8_t firstData[] = { 0x01, 0x02, 0x03 };
    static const uint8_t secondData[] = { 0xAA };
    bool firstCompleted{ false };
    bool secondCompleted{ false };

    serial->SendData(infra::MakeByteRange(firstData), [&firstCompleted]()
        {
            firstCompleted = true;
        });
    serial->SendData(infra::MakeByteRange(secondData), [&secondCompleted]()
        {
            secondCompleted = true;
        });

    EXPECT_FALSE(firstCompleted);
    EXPECT_TRUE(secondCompleted);

    ExecuteAllActions();

    EXPECT_TRUE(firstCompleted);
    ASSERT_EQ(connectionStub->sentData.size(), 3u);
    EXPECT_EQ(connectionStub->sentData[2], 0x03);
}

TEST_F(TestTcpClientSerial, queued_send_before_connect_overwrites_previous_and_completes_previous_callback)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_))
        .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
            {
                capturedClient = &factory;
            }));
    serial.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

    bool firstCompleted{ false };
    bool secondCompleted{ false };
    static const uint8_t firstData[] = { 0x01 };
    static const uint8_t secondData[] = { 0xAA, 0xBB };

    serial->SendData(infra::MakeByteRange(firstData), [&firstCompleted]()
        {
            firstCompleted = true;
        });
    serial->SendData(infra::MakeByteRange(secondData), [&secondCompleted]()
        {
            secondCompleted = true;
        });

    EXPECT_TRUE(firstCompleted);
    EXPECT_FALSE(secondCompleted);

    connectionPtr = connectionStub.Emplace();
    capturedClient->ConnectionEstablished([this](infra::SharedPtr<services::ConnectionObserver> observer)
        {
            connectionPtr->Attach(observer);
        });
    ExecuteAllActions();

    ASSERT_EQ(connectionStub->sentData.size(), 2u);
    EXPECT_EQ(connectionStub->sentData[0], 0xAA);
    EXPECT_EQ(connectionStub->sentData[1], 0xBB);
    EXPECT_TRUE(secondCompleted);
}

TEST_F(TestTcpClientSerial, connection_failed_completes_pending_callbacks)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_))
        .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
            {
                capturedClient = &factory;
            }));
    serial.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

    bool queuedCompleted{ false };
    static const uint8_t data[] = { 0x01 };
    serial->SendData(infra::MakeByteRange(data), [&queuedCompleted]()
        {
            queuedCompleted = true;
        });

    capturedClient->ConnectionFailed(services::ClientConnectionObserverFactory::ConnectFailReason::refused);

    EXPECT_TRUE(queuedCompleted);
}

TEST_F(TestTcpClientSerial, close_from_server_triggers_disconnect)
{
    CreateAndConnect();

    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    connectionPtr->AbortAndDestroy();
    connectionPtr = nullptr;

    static const uint8_t data[] = { 0x01 };
    serial->SendData(infra::MakeByteRange(data), []() {});
}

TEST_F(TestTcpClientSerial, abort_from_server_triggers_disconnect)
{
    CreateAndConnect();

    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    connectionPtr->AbortAndDestroy();
    connectionPtr = nullptr;

    static const uint8_t data[] = { 0x02 };
    serial->SendData(infra::MakeByteRange(data), []() {});
}

TEST_F(TestTcpClientSerial, receive_callback_not_set_does_not_crash)
{
    CreateAndConnect();

    const uint8_t serverData[] = { 0xAA, 0xBB };
    connectionStub->SimulateDataReceived(infra::MakeByteRange(serverData));
}

TEST_F(TestTcpClientSerial, send_while_not_connected_queues_data)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_))
        .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
            {
                capturedClient = &factory;
            }));
    serial.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

    bool callbackFired{ false };
    static const uint8_t data[] = { 0xDE, 0xAD };
    serial->SendData(infra::MakeByteRange(data), [&callbackFired]()
        {
            callbackFired = true;
        });

    EXPECT_FALSE(callbackFired);

    connectionPtr = connectionStub.Emplace();
    capturedClient->ConnectionEstablished([this](infra::SharedPtr<services::ConnectionObserver> observer)
        {
            connectionPtr->Attach(observer);
        });
    ExecuteAllActions();

    ASSERT_EQ(connectionStub->sentData.size(), 2u);
    EXPECT_EQ(connectionStub->sentData[0], 0xDE);
    EXPECT_EQ(connectionStub->sentData[1], 0xAD);
    EXPECT_TRUE(callbackFired);
}

TEST_F(TestTcpClientSerial, connection_failed_clears_queued_data_and_fires_callback)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_))
        .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
            {
                capturedClient = &factory;
            }));
    serial.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5000);

    bool callbackFired{ false };
    static const uint8_t data[] = { 0x01 };
    serial->SendData(infra::MakeByteRange(data), [&callbackFired]()
        {
            callbackFired = true;
        });

    capturedClient->ConnectionFailed(services::ClientConnectionObserverFactory::ConnectFailReason::refused);

    EXPECT_TRUE(callbackFired);
}

TEST_F(TestTcpClientSerial, send_on_active_connection_does_not_queue)
{
    CreateAndConnect();

    static const uint8_t data[] = { 0x11, 0x22, 0x33 };
    serial->SendData(infra::MakeByteRange(data), []() {});
    ExecuteAllActions();

    ASSERT_EQ(connectionStub->sentData.size(), 3u);
    EXPECT_EQ(connectionStub->sentData[0], 0x11);
    EXPECT_EQ(connectionStub->sentData[1], 0x22);
    EXPECT_EQ(connectionStub->sentData[2], 0x33);
}

TEST_F(TestTcpClientSerial, send_empty_data_when_connected_fires_callback_immediately)
{
    CreateAndConnect();

    bool callbackFired{ false };
    serial->SendData(infra::ConstByteRange{}, [&callbackFired]()
        {
            callbackFired = true;
        });

    EXPECT_TRUE(callbackFired);
    EXPECT_TRUE(connectionStub->sentData.empty());
}

TEST_F(TestTcpClientSerial, send_empty_data_when_connected_without_callback_does_not_crash)
{
    CreateAndConnect();

    serial->SendData(infra::ConstByteRange{}, infra::Function<void()>{});

    EXPECT_TRUE(connectionStub->sentData.empty());
}

TEST_F(TestTcpClientSerial, second_send_while_pending_with_null_callback_does_not_crash)
{
    CreateAndConnect();

    static const uint8_t data[] = { 0x01, 0x02 };
    serial->SendData(infra::MakeByteRange(data), []() {});

    serial->SendData(infra::MakeByteRange(data), infra::Function<void()>{});
    ExecuteAllActions();

    ASSERT_EQ(connectionStub->sentData.size(), 2u);
}

TEST_F(TestTcpClientSerial, connection_close_from_observer_path_signals_disconnect)
{
    CreateAndConnect();

    connectionPtr->Observer().Close();

    EXPECT_FALSE(serial->IsConnected());
}

TEST_F(TestTcpClientSerial, connection_abort_from_observer_path_signals_disconnect)
{
    CreateAndConnect();

    connectionPtr->Observer().Abort();

    EXPECT_FALSE(serial->IsConnected());
}

TEST_F(TestTcpClientSerial, disconnect_with_no_pending_send_is_noop_for_send_state)
{
    CreateAndConnect();

    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    connectionPtr->AbortAndDestroy();
    connectionPtr = nullptr;

    EXPECT_FALSE(serial->IsConnected());
}

TEST_F(TestTcpClientSerial, destructor_aborts_active_connection_before_handler_storage_is_destroyed)
{
    CreateAndConnect();

    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    serial.reset();
    connectionPtr = nullptr;
}
