#include "hal/interfaces/Can.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/SharedOptional.hpp"
#include "services/network/connection/test_doubles/ConnectionMock.hpp"
#include "services/network/connection/test_doubles/ConnectionStub.hpp"
#include "tools/hardware_bridge/client/can/TcpClientCanbus.hpp"
#include "tools/hardware_bridge/client/common/BridgeException.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstring>
#include <optional>

namespace
{
    class TestTcpClientCanbusFrameEncoding
        : public testing::Test
    {
    public:
        static constexpr std::size_t canFrameSize{ 16 };
        static constexpr uint32_t canEffFlag{ 0x80000000 };
        static constexpr uint32_t canErrFlag{ 0x20000000 };

        static void EncodeFrame(hal::Can::Id id, const hal::Can::Message& data, std::array<uint8_t, canFrameSize>& buffer)
        {
            std::memset(buffer.data(), 0, canFrameSize);

            uint32_t canId{ 0 };
            if (id.Is29BitId())
                canId = id.Get29BitId() | canEffFlag;
            else
                canId = id.Get11BitId();

            buffer[0] = static_cast<uint8_t>(canId & 0xFF);
            buffer[1] = static_cast<uint8_t>((canId >> 8) & 0xFF);
            buffer[2] = static_cast<uint8_t>((canId >> 16) & 0xFF);
            buffer[3] = static_cast<uint8_t>((canId >> 24) & 0xFF);
            buffer[4] = static_cast<uint8_t>(std::min(data.size(), std::size_t{ 8 }));
            std::memcpy(buffer.data() + 8, data.data(), std::min(data.size(), std::size_t{ 8 }));
        }

        static bool DecodeFrame(const uint8_t* raw, hal::Can::Id& outId, hal::Can::Message& outData)
        {
            const uint32_t canId = static_cast<uint32_t>(raw[0]) | (static_cast<uint32_t>(raw[1]) << 8) | (static_cast<uint32_t>(raw[2]) << 16) | (static_cast<uint32_t>(raw[3]) << 24);

            if (canId & canErrFlag)
                return false;

            const bool isExtended = (canId & canEffFlag) != 0;
            const uint32_t arbId = canId & 0x1FFFFFFF;

            if (isExtended)
                outId = hal::Can::Id::Create29BitId(arbId);
            else
                outId = hal::Can::Id::Create11BitId(arbId);

            const uint8_t dlc = std::min(raw[4], uint8_t{ 8 });
            outData.clear();
            for (uint8_t i = 0; i < dlc; ++i)
                outData.push_back(raw[8 + i]);

            return true;
        }

        std::array<uint8_t, canFrameSize> buffer{};
    };
}

TEST_F(TestTcpClientCanbusFrameEncoding, encode_11bit_id_sets_correct_bytes)
{
    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);
    msg.push_back(0xBB);

    TestTcpClientCanbusFrameEncoding::EncodeFrame(id, msg, buffer);

    uint32_t encodedId{ 0 };
    std::memcpy(&encodedId, buffer.data(), sizeof(uint32_t));
    EXPECT_EQ(encodedId, 0x123u);
    EXPECT_EQ(buffer[4], 2u);
    EXPECT_EQ(buffer[8], 0xAA);
    EXPECT_EQ(buffer[9], 0xBB);
}

TEST_F(TestTcpClientCanbusFrameEncoding, encode_29bit_id_sets_eff_flag)
{
    auto id = hal::Can::Id::Create29BitId(0x1ABCDEF);
    hal::Can::Message msg;
    msg.push_back(0x01);

    TestTcpClientCanbusFrameEncoding::EncodeFrame(id, msg, buffer);

    uint32_t encodedId{ 0 };
    std::memcpy(&encodedId, buffer.data(), sizeof(uint32_t));
    EXPECT_EQ(encodedId, 0x1ABCDEFu | TestTcpClientCanbusFrameEncoding::canEffFlag);
    EXPECT_EQ(buffer[4], 1u);
    EXPECT_EQ(buffer[8], 0x01);
}

TEST_F(TestTcpClientCanbusFrameEncoding, encode_empty_message_has_zero_dlc)
{
    auto id = hal::Can::Id::Create11BitId(0x100);
    hal::Can::Message msg;

    TestTcpClientCanbusFrameEncoding::EncodeFrame(id, msg, buffer);

    EXPECT_EQ(buffer[4], 0u);
}

TEST_F(TestTcpClientCanbusFrameEncoding, encode_full_8byte_message)
{
    auto id = hal::Can::Id::Create11BitId(0x7FF);
    hal::Can::Message msg;
    for (uint8_t i = 0; i < 8; ++i)
        msg.push_back(i + 1);

    TestTcpClientCanbusFrameEncoding::EncodeFrame(id, msg, buffer);

    EXPECT_EQ(buffer[4], 8u);
    for (uint8_t i = 0; i < 8; ++i)
        EXPECT_EQ(buffer[8 + i], i + 1);
}

TEST_F(TestTcpClientCanbusFrameEncoding, decode_11bit_frame_round_trips)
{
    auto originalId = hal::Can::Id::Create11BitId(0x456);
    hal::Can::Message originalMsg;
    originalMsg.push_back(0xDE);
    originalMsg.push_back(0xAD);

    TestTcpClientCanbusFrameEncoding::EncodeFrame(originalId, originalMsg, buffer);

    hal::Can::Id decodedId = hal::Can::Id::Create11BitId(0);
    hal::Can::Message decodedMsg;
    EXPECT_TRUE(TestTcpClientCanbusFrameEncoding::DecodeFrame(buffer.data(), decodedId, decodedMsg));

    EXPECT_EQ(decodedId, originalId);
    EXPECT_EQ(decodedMsg.size(), 2u);
    EXPECT_EQ(decodedMsg[0], 0xDE);
    EXPECT_EQ(decodedMsg[1], 0xAD);
}

TEST_F(TestTcpClientCanbusFrameEncoding, decode_29bit_frame_round_trips)
{
    auto originalId = hal::Can::Id::Create29BitId(0x1FFFFFF);
    hal::Can::Message originalMsg;
    for (uint8_t i = 0; i < 8; ++i)
        originalMsg.push_back(0xF0 + i);

    TestTcpClientCanbusFrameEncoding::EncodeFrame(originalId, originalMsg, buffer);

    hal::Can::Id decodedId = hal::Can::Id::Create11BitId(0);
    hal::Can::Message decodedMsg;
    EXPECT_TRUE(TestTcpClientCanbusFrameEncoding::DecodeFrame(buffer.data(), decodedId, decodedMsg));

    EXPECT_EQ(decodedId, originalId);
    EXPECT_EQ(decodedMsg.size(), 8u);
    for (uint8_t i = 0; i < 8; ++i)
        EXPECT_EQ(decodedMsg[i], 0xF0 + i);
}

TEST_F(TestTcpClientCanbusFrameEncoding, decode_clamps_dlc_to_8)
{
    std::array<uint8_t, TestTcpClientCanbusFrameEncoding::canFrameSize> raw{};
    uint32_t canId{ 0x100 };
    std::memcpy(raw.data(), &canId, sizeof(uint32_t));
    raw[4] = 15;
    raw[8] = 0xAA;

    hal::Can::Id decodedId = hal::Can::Id::Create11BitId(0);
    hal::Can::Message decodedMsg;
    EXPECT_TRUE(TestTcpClientCanbusFrameEncoding::DecodeFrame(raw.data(), decodedId, decodedMsg));

    EXPECT_EQ(decodedMsg.size(), 8u);
}

TEST_F(TestTcpClientCanbusFrameEncoding, encode_zeroes_padding_bytes)
{
    auto id = hal::Can::Id::Create11BitId(0x001);
    hal::Can::Message msg;
    msg.push_back(0xFF);

    buffer.fill(0xCC);
    TestTcpClientCanbusFrameEncoding::EncodeFrame(id, msg, buffer);

    EXPECT_EQ(buffer[5], 0u);
    EXPECT_EQ(buffer[6], 0u);
    EXPECT_EQ(buffer[7], 0u);
}

namespace
{
    class TestTcpClientCanbusConnection
        : public testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    protected:
        ~TestTcpClientCanbusConnection() override
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

            can.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5001);

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
        std::optional<tool::TcpClientCanbus> can;
    };
}

TEST_F(TestTcpClientCanbusConnection, connects_to_factory)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_));

    tool::TcpClientCanbus can(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5001);
}

TEST_F(TestTcpClientCanbusConnection, destructor_without_connection_does_not_abort)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_));

    can.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5001);
    can.reset();
}

TEST_F(TestTcpClientCanbusConnection, send_data_arrives_at_connection)
{
    CreateAndConnect();

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);
    msg.push_back(0xBB);

    can->SendData(id, msg, [](bool success)
        {
            EXPECT_TRUE(success);
        });
    ExecuteAllActions();

    ASSERT_EQ(connectionStub->sentData.size(), TestTcpClientCanbusFrameEncoding::canFrameSize);

    uint32_t encodedId{ 0 };
    std::memcpy(&encodedId, connectionStub->sentData.data(), sizeof(uint32_t));
    EXPECT_EQ(encodedId, 0x123u);
    EXPECT_EQ(connectionStub->sentData[4], 2u);
    EXPECT_EQ(connectionStub->sentData[8], 0xAA);
    EXPECT_EQ(connectionStub->sentData[9], 0xBB);
}

TEST_F(TestTcpClientCanbusConnection, send_extended_id_uses_production_eff_encoding)
{
    CreateAndConnect();

    auto id = hal::Can::Id::Create29BitId(0x1ABCDEFu);
    hal::Can::Message msg;
    msg.push_back(0x01);

    can->SendData(id, msg, [](bool success)
        {
            EXPECT_TRUE(success);
        });
    ExecuteAllActions();

    ASSERT_EQ(connectionStub->sentData.size(), TestTcpClientCanbusFrameEncoding::canFrameSize);
    uint32_t encodedId{ 0 };
    std::memcpy(&encodedId, connectionStub->sentData.data(), sizeof(uint32_t));
    EXPECT_EQ(encodedId, 0x1ABCDEFu | TestTcpClientCanbusFrameEncoding::canEffFlag);
    EXPECT_EQ(connectionStub->sentData[4], 1u);
    EXPECT_EQ(connectionStub->sentData[8], 0x01);
}

TEST_F(TestTcpClientCanbusConnection, receive_extended_frame_uses_production_eff_decoding)
{
    CreateAndConnect();

    hal::Can::Id receivedId = hal::Can::Id::Create11BitId(0);
    can->ReceiveData([&receivedId](hal::Can::Id id, const hal::Can::Message&)
        {
            receivedId = id;
        });

    std::array<uint8_t, TestTcpClientCanbusFrameEncoding::canFrameSize> frame{};
    auto sendId = hal::Can::Id::Create29BitId(0x1ABCDEFu);
    hal::Can::Message sendMsg;
    TestTcpClientCanbusFrameEncoding::EncodeFrame(sendId, sendMsg, frame);

    connectionStub->SimulateDataReceived(infra::MakeByteRange(frame));

    EXPECT_EQ(receivedId, sendId);
}

TEST_F(TestTcpClientCanbusConnection, receive_frame_triggers_callback)
{
    CreateAndConnect();

    struct Result
    {
        bool called{ false };
        hal::Can::Id id = hal::Can::Id::Create11BitId(0);
        hal::Can::Message msg;
    } result;

    can->ReceiveData([&result](hal::Can::Id id, const hal::Can::Message& data)
        {
            result.called = true;
            result.id = id;
            result.msg = data;
        });

    std::array<uint8_t, TestTcpClientCanbusFrameEncoding::canFrameSize> frame{};
    auto sendId = hal::Can::Id::Create11BitId(0x456);
    hal::Can::Message sendMsg;
    sendMsg.push_back(0xDE);
    sendMsg.push_back(0xAD);
    TestTcpClientCanbusFrameEncoding::EncodeFrame(sendId, sendMsg, frame);

    connectionStub->SimulateDataReceived(infra::MakeByteRange(frame));

    EXPECT_TRUE(result.called);
    EXPECT_EQ(result.id, sendId);
    ASSERT_EQ(result.msg.size(), 2u);
    EXPECT_EQ(result.msg[0], 0xDE);
    EXPECT_EQ(result.msg[1], 0xAD);
}

TEST_F(TestTcpClientCanbusConnection, send_data_fails_when_max_stream_size_is_too_small)
{
    CreateAndConnect();
    connectionStub->maxSendStreamSize = TestTcpClientCanbusFrameEncoding::canFrameSize - 1;

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);

    bool success{ true };
    can->SendData(id, msg, [&success](bool sendSuccess)
        {
            success = sendSuccess;
        });
    ExecuteAllActions();

    EXPECT_FALSE(success);
    EXPECT_TRUE(connectionStub->sentData.empty());
}

TEST_F(TestTcpClientCanbusConnection, second_send_while_pending_is_rejected)
{
    CreateAndConnect();
    connectionStub->maxSendStreamSize = TestTcpClientCanbusFrameEncoding::canFrameSize;

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);

    bool firstSuccess{ false };
    bool secondSuccess{ true };
    can->SendData(id, msg, [&firstSuccess](bool sendSuccess)
        {
            firstSuccess = sendSuccess;
        });
    can->SendData(id, msg, [&secondSuccess](bool sendSuccess)
        {
            secondSuccess = sendSuccess;
        });

    ExecuteAllActions();

    EXPECT_TRUE(firstSuccess);
    EXPECT_FALSE(secondSuccess);
}

TEST_F(TestTcpClientCanbusConnection, connection_failed_completes_pending_callback_with_failure)
{
    CreateAndConnect();

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);

    bool success{ true };
    can->SendData(id, msg, [&success](bool sendSuccess)
        {
            success = sendSuccess;
        });

    capturedClient->ConnectionFailed(services::ClientConnectionObserverFactory::ConnectFailReason::refused);
    ExecuteAllActions();

    EXPECT_FALSE(success);
}

TEST_F(TestTcpClientCanbusConnection, close_from_server_triggers_disconnect)
{
    CreateAndConnect();

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);

    bool callbackFired{ false };
    bool callbackSuccess{ true };
    can->SendData(id, msg, [&callbackFired, &callbackSuccess](bool sendSuccess)
        {
            callbackFired = true;
            callbackSuccess = sendSuccess;
        });

    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    connectionPtr->AbortAndDestroy();
    connectionPtr = nullptr;
    ExecuteAllActions();

    EXPECT_TRUE(callbackFired);
    EXPECT_FALSE(callbackSuccess);
}

TEST_F(TestTcpClientCanbusConnection, abort_from_server_triggers_disconnect)
{
    CreateAndConnect();

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xBB);

    bool callbackFired{ false };
    bool callbackSuccess{ true };
    can->SendData(id, msg, [&callbackFired, &callbackSuccess](bool sendSuccess)
        {
            callbackFired = true;
            callbackSuccess = sendSuccess;
        });

    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    connectionPtr->AbortAndDestroy();
    connectionPtr = nullptr;
    ExecuteAllActions();

    EXPECT_TRUE(callbackFired);
    EXPECT_FALSE(callbackSuccess);
}

TEST_F(TestTcpClientCanbusConnection, partial_frame_receive_accumulates_correctly)
{
    CreateAndConnect();

    struct PartialResult
    {
        int callCount{ 0 };
        hal::Can::Id receivedId = hal::Can::Id::Create11BitId(0);
        hal::Can::Message receivedMsg;
    } result;

    can->ReceiveData([&result](hal::Can::Id id, const hal::Can::Message& data)
        {
            ++result.callCount;
            result.receivedId = id;
            result.receivedMsg = data;
        });

    std::array<uint8_t, TestTcpClientCanbusFrameEncoding::canFrameSize> frame{};
    auto sendId = hal::Can::Id::Create11BitId(0x7FF);
    hal::Can::Message sendMsg;
    sendMsg.push_back(0x11);
    sendMsg.push_back(0x22);
    TestTcpClientCanbusFrameEncoding::EncodeFrame(sendId, sendMsg, frame);

    connectionStub->SimulateDataReceived(infra::ConstByteRange(frame.data(), frame.data() + 8));
    EXPECT_EQ(result.callCount, 0);

    connectionStub->SimulateDataReceived(infra::ConstByteRange(frame.data() + 8, frame.data() + TestTcpClientCanbusFrameEncoding::canFrameSize));
    EXPECT_EQ(result.callCount, 1);
    EXPECT_EQ(result.receivedId, sendId);
    ASSERT_EQ(result.receivedMsg.size(), 2u);
    EXPECT_EQ(result.receivedMsg[0], 0x11);
    EXPECT_EQ(result.receivedMsg[1], 0x22);
}

TEST_F(TestTcpClientCanbusConnection, multiple_frames_in_one_receive_all_dispatched)
{
    CreateAndConnect();

    struct MultiResult
    {
        int callCount{ 0 };
        std::vector<hal::Can::Id> receivedIds;
        std::vector<hal::Can::Message> receivedMsgs;
    } result;

    can->ReceiveData([&result](hal::Can::Id id, const hal::Can::Message& data)
        {
            ++result.callCount;
            result.receivedIds.push_back(id);
            result.receivedMsgs.push_back(data);
        });

    std::array<uint8_t, TestTcpClientCanbusFrameEncoding::canFrameSize> frame1{};
    auto id1 = hal::Can::Id::Create11BitId(0x111);
    hal::Can::Message msg1;
    msg1.push_back(0xAA);
    TestTcpClientCanbusFrameEncoding::EncodeFrame(id1, msg1, frame1);

    std::array<uint8_t, TestTcpClientCanbusFrameEncoding::canFrameSize> frame2{};
    auto id2 = hal::Can::Id::Create11BitId(0x222);
    hal::Can::Message msg2;
    msg2.push_back(0xBB);
    TestTcpClientCanbusFrameEncoding::EncodeFrame(id2, msg2, frame2);

    std::array<uint8_t, 2 * TestTcpClientCanbusFrameEncoding::canFrameSize> combined{};
    std::memcpy(combined.data(), frame1.data(), TestTcpClientCanbusFrameEncoding::canFrameSize);
    std::memcpy(combined.data() + TestTcpClientCanbusFrameEncoding::canFrameSize, frame2.data(), TestTcpClientCanbusFrameEncoding::canFrameSize);

    connectionStub->SimulateDataReceived(infra::MakeByteRange(combined));

    EXPECT_EQ(result.callCount, 2);
    ASSERT_EQ(result.receivedIds.size(), 2u);
    EXPECT_EQ(result.receivedIds[0], id1);
    EXPECT_EQ(result.receivedIds[1], id2);
    ASSERT_EQ(result.receivedMsgs[0].size(), 1u);
    EXPECT_EQ(result.receivedMsgs[0][0], 0xAA);
    ASSERT_EQ(result.receivedMsgs[1].size(), 1u);
    EXPECT_EQ(result.receivedMsgs[1][0], 0xBB);
}

TEST_F(TestTcpClientCanbusConnection, malformed_frame_closes_connection)
{
    CreateAndConnect();

    bool callbackCalled{ false };
    can->ReceiveData([&callbackCalled](hal::Can::Id, const hal::Can::Message&)
        {
            callbackCalled = true;
        });

    std::array<uint8_t, TestTcpClientCanbusFrameEncoding::canFrameSize> frame{};
    uint32_t errCanId{ TestTcpClientCanbusFrameEncoding::canErrFlag };
    std::memcpy(frame.data(), &errCanId, sizeof(uint32_t));
    frame[4] = 1;
    frame[8] = 0xFF;

    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    EXPECT_THROW(connectionStub->SimulateDataReceived(infra::MakeByteRange(frame)), tool::BridgeConnectionException);
    connectionPtr = nullptr;

    EXPECT_FALSE(callbackCalled);
}

TEST_F(TestTcpClientCanbusConnection, send_while_not_connected_fails_immediately)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_))
        .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
            {
                capturedClient = &factory;
            }));
    can.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5001);

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);

    bool callbackFired{ false };
    bool callbackSuccess{ true };
    can->SendData(id, msg, [&callbackFired, &callbackSuccess](bool sendSuccess)
        {
            callbackFired = true;
            callbackSuccess = sendSuccess;
        });

    EXPECT_TRUE(callbackFired);
    EXPECT_FALSE(callbackSuccess);
}

TEST_F(TestTcpClientCanbusConnection, send_while_not_connected_without_callback_does_not_crash)
{
    EXPECT_CALL(connectionFactory, Connect(testing::_))
        .WillOnce(testing::Invoke([this](services::ClientConnectionObserverFactory& factory)
            {
                capturedClient = &factory;
            }));
    can.emplace(connectionFactory, services::IPv4Address{ 127, 0, 0, 1 }, 5001);

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);

    can->SendData(id, msg, infra::Function<void(bool)>{});
}

TEST_F(TestTcpClientCanbusConnection, second_send_while_pending_with_null_callback_does_not_crash)
{
    CreateAndConnect();

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);

    can->SendData(id, msg, [](bool) {});
    can->SendData(id, msg, infra::Function<void(bool)>{});
    ExecuteAllActions();
}

TEST_F(TestTcpClientCanbusConnection, send_fails_when_max_stream_size_too_small_without_callback)
{
    CreateAndConnect();
    connectionStub->maxSendStreamSize = TestTcpClientCanbusFrameEncoding::canFrameSize - 1;

    auto id = hal::Can::Id::Create11BitId(0x123);
    hal::Can::Message msg;
    msg.push_back(0xAA);

    can->SendData(id, msg, infra::Function<void(bool)>{});

    EXPECT_TRUE(connectionStub->sentData.empty());
}

TEST_F(TestTcpClientCanbusConnection, receive_frame_without_callback_does_not_crash)
{
    CreateAndConnect();

    std::array<uint8_t, TestTcpClientCanbusFrameEncoding::canFrameSize> frame{};
    auto sendId = hal::Can::Id::Create11BitId(0x100);
    hal::Can::Message sendMsg;
    sendMsg.push_back(0x55);
    TestTcpClientCanbusFrameEncoding::EncodeFrame(sendId, sendMsg, frame);

    connectionStub->SimulateDataReceived(infra::MakeByteRange(frame));
}

TEST_F(TestTcpClientCanbusConnection, connection_close_from_observer_path_signals_disconnect)
{
    CreateAndConnect();

    connectionPtr->Observer().Close();

    EXPECT_FALSE(can->IsConnected());
}

TEST_F(TestTcpClientCanbusConnection, connection_abort_from_observer_path_signals_disconnect)
{
    CreateAndConnect();

    connectionPtr->Observer().Abort();

    EXPECT_FALSE(can->IsConnected());
}

TEST_F(TestTcpClientCanbusConnection, disconnect_with_no_pending_send_is_noop_for_fail_state)
{
    CreateAndConnect();

    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    connectionPtr->AbortAndDestroy();
    connectionPtr = nullptr;

    EXPECT_FALSE(can->IsConnected());
}

TEST_F(TestTcpClientCanbusConnection, destructor_aborts_active_connection_before_handler_storage_is_destroyed)
{
    CreateAndConnect();

    EXPECT_CALL(*connectionPtr, AbortAndDestroyMock);
    can.reset();
    connectionPtr = nullptr;
}
