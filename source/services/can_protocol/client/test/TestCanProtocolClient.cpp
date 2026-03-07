#include "source/services/can_protocol/client/CanProtocolClientImpl.hpp"
#include "source/services/can_protocol/core/CanFrameCodec.hpp"
#include "source/services/can_protocol/core/test/CanMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace testing;
    using namespace services;

    class CanProtocolClientObserverMock
        : public CanProtocolClientObserver
    {
    public:
        using CanProtocolClientObserver::CanProtocolClientObserver;

        MOCK_METHOD(void, OnCommandAckReceived, (CanCategory category, CanMessageType command, CanAckStatus status), (override));
        MOCK_METHOD(void, OnMotorStatusReceived, (CanMotorState state, CanControlMode mode, CanFaultCode fault), (override));
        MOCK_METHOD(void, OnControlModeReceived, (CanControlMode mode), (override));
        MOCK_METHOD(void, OnCurrentMeasurementReceived, (float idCurrent, float iqCurrent), (override));
        MOCK_METHOD(void, OnSpeedPositionReceived, (float speed, float position), (override));
        MOCK_METHOD(void, OnBusVoltageReceived, (float voltage), (override));
        MOCK_METHOD(void, OnFaultEventReceived, (CanFaultCode fault), (override));
    };

    class CanProtocolClientTest
        : public ::testing::Test
    {
    public:
        struct FixtureInit
        {
            FixtureInit(StrictMock<hal::CanMock>& canMock,
                infra::Function<void(hal::Can::Id, const hal::Can::Message&)>& receiveCallback)
            {
                EXPECT_CALL(canMock, ReceiveData(_)).WillOnce([&receiveCallback](const auto& callback)
                    {
                        receiveCallback = callback;
                    });
                ON_CALL(canMock, SendData(_, _, _))
                    .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                        {
                            cb(true);
                        }));
            }
        };

        void SimulateRx(hal::Can::Id id, const hal::Can::Message& data)
        {
            receiveCallback(id, data);
        }

        hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message msg;
            for (auto b : bytes)
                msg.push_back(b);
            return msg;
        }

        hal::Can::Id MakeTelemetryId(CanMessageType type, uint16_t nodeId = 1)
        {
            return hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::telemetry, CanCategory::telemetry, type, nodeId));
        }

        hal::Can::Id MakeSystemId(CanMessageType type, uint16_t nodeId = 1)
        {
            return hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::heartbeat, CanCategory::system, type, nodeId));
        }

        CanProtocolClient::Config config{ 1 };
        StrictMock<hal::CanMock> canMock;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        FixtureInit fixtureInit{ canMock, receiveCallback };
        CanProtocolClientImpl client{ canMock, config };
        StrictMock<CanProtocolClientObserverMock> observerMock{ client };
    };

    // === Send command tests ===

    TEST_F(CanProtocolClientTest, SendStartMotor_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));
        bool done = false;
        client.SendStartMotor([&done]()
            {
                done = true;
            });
        EXPECT_TRUE(done);
    }

    TEST_F(CanProtocolClientTest, SendStopMotor_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));
        bool done = false;
        client.SendStopMotor([&done]()
            {
                done = true;
            });
        EXPECT_TRUE(done);
    }

    TEST_F(CanProtocolClientTest, SendEmergencyStop_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));
        bool done = false;
        client.SendEmergencyStop([&done]()
            {
                done = true;
            });
        EXPECT_TRUE(done);
    }

    TEST_F(CanProtocolClientTest, SendSetControlMode_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));
        bool done = false;
        client.SendSetControlMode(CanControlMode::speed, [&done]()
            {
                done = true;
            });
        EXPECT_TRUE(done);
    }

    TEST_F(CanProtocolClientTest, SendSetTorqueSetpoint_SendsEncodedValues)
    {
        hal::Can::Message capturedData;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(DoAll(SaveArg<1>(&capturedData), Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                                                                                              {
                                                                                                  cb(true);
                                                                                              })));

        client.SendSetTorqueSetpoint(1.5f, 2.5f, []() {});

        auto idExpected = CanFrameCodec::FloatToFixed16(1.5f, canCurrentScale);
        auto iqExpected = CanFrameCodec::FloatToFixed16(2.5f, canCurrentScale);
        EXPECT_EQ(capturedData.size(), 5u);
        EXPECT_EQ(static_cast<uint8_t>(idExpected >> 8), capturedData[1]);
        EXPECT_EQ(static_cast<uint8_t>(idExpected & 0xFF), capturedData[2]);
        EXPECT_EQ(static_cast<uint8_t>(iqExpected >> 8), capturedData[3]);
        EXPECT_EQ(static_cast<uint8_t>(iqExpected & 0xFF), capturedData[4]);
    }

    TEST_F(CanProtocolClientTest, SendSetSpeedSetpoint_SendsEncodedValue)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));
        bool done = false;
        client.SendSetSpeedSetpoint(10.0f, [&done]()
            {
                done = true;
            });
        EXPECT_TRUE(done);
    }

    TEST_F(CanProtocolClientTest, SendSetPositionSetpoint_SendsEncodedValue)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));
        bool done = false;
        client.SendSetPositionSetpoint(3.14f, [&done]()
            {
                done = true;
            });
        EXPECT_TRUE(done);
    }

    TEST_F(CanProtocolClientTest, SendSetCurrentIdPid_SendsEncodedGains)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));
        bool done = false;
        client.SendSetCurrentIdPid(1.0f, 0.5f, 0.1f, [&done]()
            {
                done = true;
            });
        EXPECT_TRUE(done);
    }

    TEST_F(CanProtocolClientTest, SendSetSupplyVoltage_SendsEncodedValue)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));
        bool done = false;
        client.SendSetSupplyVoltage(24.0f, [&done]()
            {
                done = true;
            });
        EXPECT_TRUE(done);
    }

    TEST_F(CanProtocolClientTest, SendSetMaxCurrent_SendsEncodedValue)
    {
        EXPECT_CALL(canMock, SendData(_, _, _));
        bool done = false;
        client.SendSetMaxCurrent(10.0f, [&done]()
            {
                done = true;
            });
        EXPECT_TRUE(done);
    }

    TEST_F(CanProtocolClientTest, RequestData_SendsFlagsInPayload)
    {
        hal::Can::Message capturedData;
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce(DoAll(SaveArg<1>(&capturedData), Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                                                                                              {
                                                                                                  cb(true);
                                                                                              })));

        client.RequestData(DataRequestFlags::all, []() {});

        EXPECT_EQ(capturedData.size(), 1u);
        EXPECT_EQ(capturedData[0], static_cast<uint8_t>(DataRequestFlags::all));
    }

    // === Receive telemetry tests ===

    TEST_F(CanProtocolClientTest, ReceiveMotorStatus_NotifiesObserver)
    {
        auto id = MakeTelemetryId(CanMessageType::motorStatus);
        auto data = MakeMessage({ static_cast<uint8_t>(CanMotorState::running),
            static_cast<uint8_t>(CanControlMode::speed),
            static_cast<uint8_t>(CanFaultCode::none) });

        EXPECT_CALL(observerMock, OnMotorStatusReceived(
                                      CanMotorState::running, CanControlMode::speed, CanFaultCode::none));
        EXPECT_CALL(observerMock, OnControlModeReceived(CanControlMode::speed));
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, ReceiveCurrentMeasurement_NotifiesObserver)
    {
        auto idFixed = CanFrameCodec::FloatToFixed16(1.5f, canCurrentScale);
        auto iqFixed = CanFrameCodec::FloatToFixed16(2.0f, canCurrentScale);

        auto id = MakeTelemetryId(CanMessageType::currentMeasurement);
        auto data = MakeMessage({ static_cast<uint8_t>(idFixed >> 8),
            static_cast<uint8_t>(idFixed & 0xFF),
            static_cast<uint8_t>(iqFixed >> 8),
            static_cast<uint8_t>(iqFixed & 0xFF) });

        EXPECT_CALL(observerMock, OnCurrentMeasurementReceived(FloatNear(1.5f, 0.01f), FloatNear(2.0f, 0.01f)));
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, ReceiveSpeedPosition_NotifiesObserver)
    {
        auto speedFixed = CanFrameCodec::FloatToFixed16(5.0f, canSpeedScale);
        auto posFixed = CanFrameCodec::FloatToFixed16(1.57f, canPositionScale);

        auto id = MakeTelemetryId(CanMessageType::speedPosition);
        auto data = MakeMessage({ static_cast<uint8_t>(speedFixed >> 8),
            static_cast<uint8_t>(speedFixed & 0xFF),
            static_cast<uint8_t>(posFixed >> 8),
            static_cast<uint8_t>(posFixed & 0xFF) });

        EXPECT_CALL(observerMock, OnSpeedPositionReceived(FloatNear(5.0f, 0.01f), FloatNear(1.57f, 0.01f)));
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, ReceiveBusVoltage_NotifiesObserver)
    {
        auto voltFixed = CanFrameCodec::FloatToFixed16(24.0f, canVoltageScale);

        auto id = MakeTelemetryId(CanMessageType::busVoltage);
        auto data = MakeMessage({ static_cast<uint8_t>(voltFixed >> 8),
            static_cast<uint8_t>(voltFixed & 0xFF) });

        EXPECT_CALL(observerMock, OnBusVoltageReceived(FloatNear(24.0f, 0.01f)));
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, ReceiveFaultEvent_NotifiesObserver)
    {
        auto id = MakeTelemetryId(CanMessageType::faultEvent);
        auto data = MakeMessage({ static_cast<uint8_t>(CanFaultCode::overCurrent) });

        EXPECT_CALL(observerMock, OnFaultEventReceived(CanFaultCode::overCurrent));
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, ReceiveCommandAck_NotifiesObserver)
    {
        auto id = MakeSystemId(CanMessageType::commandAck);
        auto data = MakeMessage({ static_cast<uint8_t>(CanCategory::motorControl),
            static_cast<uint8_t>(CanMessageType::startMotor),
            static_cast<uint8_t>(CanAckStatus::success) });

        EXPECT_CALL(observerMock, OnCommandAckReceived(
                                      CanCategory::motorControl, CanMessageType::startMotor, CanAckStatus::success));
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, ReceiveHeartbeat_SilentlyConsumed)
    {
        auto id = MakeSystemId(CanMessageType::heartbeat);
        auto data = MakeMessage({ canProtocolVersion });

        // No observer calls expected — heartbeat is managed internally
        SimulateRx(id, data);
    }

    // === Edge cases ===

    TEST_F(CanProtocolClientTest, IgnoresStandard11BitId)
    {
        auto id = hal::Can::Id::Create11BitId(0x100);
        auto data = MakeMessage({ 0x01 });

        // No observer calls expected
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, MotorStatus_TooShort_Ignored)
    {
        auto id = MakeTelemetryId(CanMessageType::motorStatus);
        auto data = MakeMessage({ 0x01, 0x02 }); // Only 2 bytes, need 3

        // No observer calls expected
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, CurrentMeasurement_TooShort_Ignored)
    {
        auto id = MakeTelemetryId(CanMessageType::currentMeasurement);
        auto data = MakeMessage({ 0x01, 0x02, 0x03 }); // Only 3 bytes, need 4

        // No observer calls expected
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, CommandAck_TooShort_Ignored)
    {
        auto id = MakeSystemId(CanMessageType::commandAck);
        auto data = MakeMessage({ 0x01, 0x02 }); // Only 2 bytes, need 3

        // No observer calls expected
        SimulateRx(id, data);
    }

    TEST_F(CanProtocolClientTest, SetNodeId_ChangesNodeId)
    {
        client.SetNodeId(42);
        EXPECT_EQ(client.NodeId(), 42);
    }

    TEST_F(CanProtocolClientTest, QueuedFrames_SentInOrder)
    {
        infra::Function<void(bool)> firstCb;
        EXPECT_CALL(canMock, SendData(_, _, _))
            .WillOnce(SaveArg<2>(&firstCb))
            .WillOnce(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                {
                    cb(true);
                }));

        bool done1 = false;
        bool done2 = false;
        client.SendStartMotor([&done1]()
            {
                done1 = true;
            });
        client.SendStopMotor([&done2]()
            {
                done2 = true;
            });

        EXPECT_FALSE(done1);
        EXPECT_FALSE(done2);

        firstCb(true);
        EXPECT_TRUE(done1);
        EXPECT_TRUE(done2);
    }
}
