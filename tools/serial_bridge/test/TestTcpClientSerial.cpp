#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/SharedOptional.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/network/connection/test_doubles/ConnectionMock.hpp"
#include "services/network/connection/test_doubles/ConnectionStub.hpp"
#include "tools/serial_bridge/lib/TcpClientSerial.hpp"
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
