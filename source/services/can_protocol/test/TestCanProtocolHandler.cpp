#include "source/services/can_protocol/CanFrameCodec.hpp"
#include "source/services/can_protocol/CanProtocolHandlerImpl.hpp"
#include "source/services/can_protocol/test/CanMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace testing;
    using namespace services;

    class CanProtocolHandlerObserverMock
        : public CanProtocolHandlerObserver
    {
    public:
        using CanProtocolHandlerObserver::CanProtocolHandlerObserver;

        MOCK_METHOD(void, OnMotorStart, (), (override));
        MOCK_METHOD(void, OnMotorStop, (), (override));
        MOCK_METHOD(void, OnEmergencyStop, (), (override));
        MOCK_METHOD(void, OnControlModeChanged, (CanControlMode mode), (override));
        MOCK_METHOD(void, OnTorqueSetpoint, (float idCurrent, float iqCurrent), (override));
        MOCK_METHOD(void, OnSpeedSetpoint, (float speedRadPerSec), (override));
        MOCK_METHOD(void, OnPositionSetpoint, (float positionRad), (override));

        MOCK_METHOD(void, OnCurrentIdPidChanged, (float kp, float ki, float kd), (override));
        MOCK_METHOD(void, OnCurrentIqPidChanged, (float kp, float ki, float kd), (override));
        MOCK_METHOD(void, OnSpeedPidChanged, (float kp, float ki, float kd), (override));
        MOCK_METHOD(void, OnPositionPidChanged, (float kp, float ki, float kd), (override));

        MOCK_METHOD(void, OnPolePairsChanged, (uint8_t polePairs), (override));
        MOCK_METHOD(void, OnResistanceChanged, (float resistanceOhm), (override));
        MOCK_METHOD(void, OnInductanceChanged, (float inductanceHenry), (override));
        MOCK_METHOD(void, OnFluxLinkageChanged, (float fluxWeber), (override));

        MOCK_METHOD(void, OnSupplyVoltageChanged, (float voltageVolts), (override));
        MOCK_METHOD(void, OnMaxCurrentChanged, (float currentAmps), (override));

        MOCK_METHOD(void, OnHeartbeatReceived, (uint8_t protocolVersion), (override));
        MOCK_METHOD(void, OnStatusRequested, (), (override));
    };

    class CanProtocolHandlerTest
        : public ::testing::Test
    {
    public:
        CanProtocolHandlerTest()
            : handler(canMock, config)
            , observerMock(handler)
        {}

        hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes)
        {
            hal::Can::Message msg;
            for (auto b : bytes)
                msg.push_back(b);
            return msg;
        }

        hal::Can::Id MakeCommandId(CanCategory category, CanMessageType type, uint16_t nodeId = 1)
        {
            return hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::command, category, type, nodeId));
        }

        hal::Can::Id MakeEmergencyId(CanMessageType type, uint16_t nodeId = 1)
        {
            return hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::emergency, CanCategory::motorControl, type, nodeId));
        }

        hal::Can::Id MakeSystemId(CanMessageType type, uint16_t nodeId = 1)
        {
            return hal::Can::Id::Create29BitId(
                MakeCanId(CanPriority::heartbeat, CanCategory::system, type, nodeId));
        }

        void WriteInt16(hal::Can::Message& msg, std::size_t offset, int16_t value)
        {
            while (msg.size() < offset + 2)
                msg.push_back(0);
            msg[offset] = static_cast<uint8_t>(value >> 8);
            msg[offset + 1] = static_cast<uint8_t>(value & 0xFF);
        }

        void WriteInt32(hal::Can::Message& msg, std::size_t offset, int32_t value)
        {
            while (msg.size() < offset + 4)
                msg.push_back(0);
            msg[offset] = static_cast<uint8_t>((value >> 24) & 0xFF);
            msg[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
            msg[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
            msg[offset + 3] = static_cast<uint8_t>(value & 0xFF);
        }

        CanProtocolHandler::Config config{ 1, 500 };
        StrictMock<hal::CanMock> canMock;
        CanProtocolHandlerImpl handler;
        StrictMock<CanProtocolHandlerObserverMock> observerMock;
    };

    TEST_F(CanProtocolHandlerTest, StartMotor_NotifiesObserver)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart());
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, StopMotor_NotifiesObserver)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::stopMotor);

        EXPECT_CALL(observerMock, OnMotorStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, EmergencyStop_NotifiesObserverWithoutSequenceCheck)
    {
        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(observerMock, OnEmergencyStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 99 }));
    }

    TEST_F(CanProtocolHandlerTest, SetControlMode_Speed)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setControlMode);

        EXPECT_CALL(observerMock, OnControlModeChanged(CanControlMode::speed));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1, static_cast<uint8_t>(CanControlMode::speed) }));
    }

    TEST_F(CanProtocolHandlerTest, SetControlMode_InvalidMode_RejectsWithInvalidPayload)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setControlMode);

        EXPECT_CALL(observerMock, OnControlModeChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1, 0xFF }));
    }

    TEST_F(CanProtocolHandlerTest, SetTorqueSetpoint_DecodesFixedPoint)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setTorqueSetpoint);
        hal::Can::Message msg;
        msg.push_back(1);
        WriteInt16(msg, 1, 0);
        WriteInt16(msg, 3, 1500);

        EXPECT_CALL(observerMock, OnTorqueSetpoint(FloatNear(0.0f, 0.01f), FloatNear(1.5f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetSpeedSetpoint_DecodesFixedPoint)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setSpeedSetpoint);
        hal::Can::Message msg;
        msg.push_back(1);
        WriteInt32(msg, 1, 10000);

        EXPECT_CALL(observerMock, OnSpeedSetpoint(FloatNear(10.0f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetPositionSetpoint_DecodesFixedPoint)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setPositionSetpoint);
        hal::Can::Message msg;
        msg.push_back(1);
        WriteInt32(msg, 1, 31416);

        EXPECT_CALL(observerMock, OnPositionSetpoint(FloatNear(3.1416f, 0.001f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetCurrentIdPid_DecodesGains)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setCurrentIdPid);
        hal::Can::Message msg;
        msg.push_back(1);
        WriteInt16(msg, 1, 1200);
        WriteInt16(msg, 3, 500);
        WriteInt16(msg, 5, 100);

        EXPECT_CALL(observerMock, OnCurrentIdPidChanged(FloatNear(1.2f, 0.01f), FloatNear(0.5f, 0.01f), FloatNear(0.1f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetCurrentIqPid_DecodesGains)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setCurrentIqPid);
        hal::Can::Message msg;
        msg.push_back(2);
        WriteInt16(msg, 1, 3000);
        WriteInt16(msg, 3, 100);
        WriteInt16(msg, 5, 50);

        EXPECT_CALL(observerMock, OnCurrentIqPidChanged(FloatNear(3.0f, 0.01f), FloatNear(0.1f, 0.01f), FloatNear(0.05f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetSpeedPid_DecodesGains)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setSpeedPid);
        hal::Can::Message msg;
        msg.push_back(3);
        WriteInt16(msg, 1, 2000);
        WriteInt16(msg, 3, 300);
        WriteInt16(msg, 5, 0);

        EXPECT_CALL(observerMock, OnSpeedPidChanged(FloatNear(2.0f, 0.01f), FloatNear(0.3f, 0.01f), FloatNear(0.0f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetPositionPid_DecodesGains)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setPositionPid);
        hal::Can::Message msg;
        msg.push_back(4);
        WriteInt16(msg, 1, 5000);
        WriteInt16(msg, 3, 1000);
        WriteInt16(msg, 5, 200);

        EXPECT_CALL(observerMock, OnPositionPidChanged(FloatNear(5.0f, 0.01f), FloatNear(1.0f, 0.01f), FloatNear(0.2f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, PidCommand_TooShortPayload_RejectsWithInvalidPayload)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setCurrentIdPid);

        EXPECT_CALL(observerMock, OnCurrentIdPidChanged(_, _, _)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1, 0x01 }));
    }

    TEST_F(CanProtocolHandlerTest, SetPolePairs)
    {
        auto id = MakeCommandId(CanCategory::motorParameters, CanMessageType::setPolePairs);

        EXPECT_CALL(observerMock, OnPolePairsChanged(7));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1, 7 }));
    }

    TEST_F(CanProtocolHandlerTest, SetResistance_DecodesFixedPoint)
    {
        auto id = MakeCommandId(CanCategory::motorParameters, CanMessageType::setResistance);
        hal::Can::Message msg;
        msg.push_back(2);
        WriteInt32(msg, 1, 50000);

        EXPECT_CALL(observerMock, OnResistanceChanged(FloatNear(0.5f, 0.001f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetInductance_DecodesFixedPoint)
    {
        auto id = MakeCommandId(CanCategory::motorParameters, CanMessageType::setInductance);
        hal::Can::Message msg;
        msg.push_back(3);
        WriteInt32(msg, 1, 1000);

        EXPECT_CALL(observerMock, OnInductanceChanged(FloatNear(0.0001f, 0.00001f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetFluxLinkage_DecodesFixedPoint)
    {
        auto id = MakeCommandId(CanCategory::motorParameters, CanMessageType::setFluxLinkage);
        hal::Can::Message msg;
        msg.push_back(4);
        WriteInt32(msg, 1, 8500);

        EXPECT_CALL(observerMock, OnFluxLinkageChanged(FloatNear(0.0085f, 0.0001f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetSupplyVoltage)
    {
        auto id = MakeCommandId(CanCategory::systemParameters, CanMessageType::setSupplyVoltage);
        hal::Can::Message msg;
        msg.push_back(5);
        WriteInt16(msg, 1, 2400);

        EXPECT_CALL(observerMock, OnSupplyVoltageChanged(FloatNear(24.0f, 0.1f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetMaxCurrent)
    {
        auto id = MakeCommandId(CanCategory::systemParameters, CanMessageType::setMaxCurrent);
        hal::Can::Message msg;
        msg.push_back(6);
        WriteInt16(msg, 1, 10000);

        EXPECT_CALL(observerMock, OnMaxCurrentChanged(FloatNear(10.0f, 0.1f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, HeartbeatReceived_NotifiesObserver)
    {
        auto id = MakeSystemId(CanMessageType::heartbeat);

        EXPECT_CALL(observerMock, OnHeartbeatReceived(canProtocolVersion));

        handler.ProcessReceivedMessage(id, MakeMessage({ canProtocolVersion }));
    }

    TEST_F(CanProtocolHandlerTest, RequestStatus_NotifiesObserver)
    {
        auto id = MakeSystemId(CanMessageType::requestStatus);

        EXPECT_CALL(observerMock, OnStatusRequested());

        handler.ProcessReceivedMessage(id, MakeMessage({}));
    }

    TEST_F(CanProtocolHandlerTest, RejectsMessageForDifferentNode)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor, 99);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(0);

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, AcceptsBroadcastMessage)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::emergencyStop, canBroadcastNodeId);

        EXPECT_CALL(observerMock, OnEmergencyStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, Rejects11BitId)
    {
        auto id = hal::Can::Id::Create11BitId(0x100);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(0);

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, RateLimiting_RejectsExcessMessages)
    {
        CanProtocolHandler::Config limitedConfig{ 1, 3 };
        StrictMock<hal::CanMock> limitedCan;
        CanProtocolHandlerImpl limitedHandler(limitedCan, limitedConfig);
        StrictMock<CanProtocolHandlerObserverMock> limitedObserver(limitedHandler);

        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(limitedObserver, OnEmergencyStop()).Times(3);
        EXPECT_CALL(limitedCan, SendData(_, _, _)).Times(3);

        for (int i = 0; i < 3; ++i)
            limitedHandler.ProcessReceivedMessage(id, MakeMessage({ 1 }));

        limitedHandler.ProcessReceivedMessage(id, MakeMessage({ 1 }));

        EXPECT_TRUE(limitedHandler.IsRateLimited());
    }

    TEST_F(CanProtocolHandlerTest, RateLimiting_ResetsCounter)
    {
        CanProtocolHandler::Config limitedConfig{ 1, 2 };
        StrictMock<hal::CanMock> limitedCan;
        CanProtocolHandlerImpl limitedHandler(limitedCan, limitedConfig);
        StrictMock<CanProtocolHandlerObserverMock> limitedObserver(limitedHandler);

        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(limitedObserver, OnEmergencyStop()).Times(2);
        EXPECT_CALL(limitedCan, SendData(_, _, _)).Times(2);

        limitedHandler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
        limitedHandler.ProcessReceivedMessage(id, MakeMessage({ 1 }));

        EXPECT_TRUE(limitedHandler.IsRateLimited());

        limitedHandler.ResetRateCounter();
        EXPECT_FALSE(limitedHandler.IsRateLimited());
    }

    TEST_F(CanProtocolHandlerTest, SequenceValidation_RejectsDuplicate)
    {
        auto id1 = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(1);
        EXPECT_CALL(canMock, SendData(_, _, _)).Times(1);

        handler.ProcessReceivedMessage(id1, MakeMessage({ 1 }));

        auto id2 = MakeCommandId(CanCategory::motorControl, CanMessageType::stopMotor);

        EXPECT_CALL(canMock, SendData(_, _, _)).Times(1);

        handler.ProcessReceivedMessage(id2, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, SequenceValidation_AcceptsSequentialMessages)
    {
        auto idStart = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);
        auto idStop = MakeCommandId(CanCategory::motorControl, CanMessageType::stopMotor);

        EXPECT_CALL(observerMock, OnMotorStart());
        EXPECT_CALL(observerMock, OnMotorStop());
        EXPECT_CALL(canMock, SendData(_, _, _)).Times(2);

        handler.ProcessReceivedMessage(idStart, MakeMessage({ 1 }));
        handler.ProcessReceivedMessage(idStop, MakeMessage({ 2 }));
    }

    TEST_F(CanProtocolHandlerTest, SequenceWrapsAround)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(2);
        EXPECT_CALL(canMock, SendData(_, _, _)).Times(2);

        handler.ProcessReceivedMessage(id, MakeMessage({ 255 }));
        handler.ProcessReceivedMessage(id, MakeMessage({ 0 }));
    }

    TEST_F(CanProtocolHandlerTest, UnknownCategory_SilentlyDiscarded)
    {
        uint32_t rawId = MakeCanId(CanPriority::command, static_cast<CanCategory>(0xF), CanMessageType::startMotor, 1);
        auto id = hal::Can::Id::Create29BitId(rawId);

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, SendMotorStatus_SendsCorrectFrame)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto&)
            {
                EXPECT_TRUE(id.Is29BitId());
                ASSERT_GE(data.size(), 3u);
                EXPECT_EQ(data[0], static_cast<uint8_t>(CanMotorState::running));
                EXPECT_EQ(data[1], static_cast<uint8_t>(CanControlMode::speed));
                EXPECT_EQ(data[2], static_cast<uint8_t>(CanFaultCode::none));
            });

        handler.SendMotorStatus(CanMotorState::running, CanControlMode::speed, CanFaultCode::none);
    }

    TEST_F(CanProtocolHandlerTest, SendCurrentMeasurement_EncodesFixedPoint)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto&)
            {
                ASSERT_GE(data.size(), 4u);
                int16_t iq = static_cast<int16_t>((static_cast<uint16_t>(data[2]) << 8) | data[3]);
                EXPECT_EQ(iq, 1500);
            });

        handler.SendCurrentMeasurement(0.0f, 1.5f);
    }

    TEST_F(CanProtocolHandlerTest, SendSpeedPosition_EncodesCorrectly)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto&)
            {
                ASSERT_GE(data.size(), 4u);
            });

        handler.SendSpeedPosition(10.0f, 3.14f);
    }

    TEST_F(CanProtocolHandlerTest, SendBusVoltage_EncodesCorrectly)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto&)
            {
                ASSERT_GE(data.size(), 2u);
                int16_t voltage = static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
                EXPECT_EQ(voltage, 2400);
            });

        handler.SendBusVoltage(24.0f);
    }

    TEST_F(CanProtocolHandlerTest, SendHeartbeat_IncludesProtocolVersion)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto&)
            {
                ASSERT_GE(data.size(), 1u);
                EXPECT_EQ(data[0], canProtocolVersion);
            });

        handler.SendHeartbeat();
    }

    TEST_F(CanProtocolHandlerTest, SendFaultEvent_UsesEmergencyPriority)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id id, const hal::Can::Message& data, const auto&)
            {
                EXPECT_TRUE(id.Is29BitId());
                auto rawId = id.Get29BitId();
                EXPECT_EQ(ExtractCanPriority(rawId), CanPriority::emergency);
                ASSERT_GE(data.size(), 1u);
                EXPECT_EQ(data[0], static_cast<uint8_t>(CanFaultCode::overCurrent));
            });

        handler.SendFaultEvent(CanFaultCode::overCurrent);
    }

    TEST_F(CanProtocolHandlerTest, SendCommandAck_IncludesCategoryCommandTypeAndStatus)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto&)
            {
                ASSERT_GE(data.size(), 3u);
                EXPECT_EQ(data[0], static_cast<uint8_t>(CanCategory::motorControl));
                EXPECT_EQ(data[1], static_cast<uint8_t>(CanMessageType::startMotor));
                EXPECT_EQ(data[2], static_cast<uint8_t>(CanAckStatus::success));
            });

        handler.SendCommandAck(CanCategory::motorControl, CanMessageType::startMotor, CanAckStatus::success);
    }

    TEST_F(CanProtocolHandlerTest, CanIdRoundTrip)
    {
        uint32_t id = MakeCanId(CanPriority::command, CanCategory::motorControl, CanMessageType::startMotor, 42);

        EXPECT_EQ(ExtractCanPriority(id), CanPriority::command);
        EXPECT_EQ(ExtractCanCategory(id), CanCategory::motorControl);
        EXPECT_EQ(ExtractCanMessageType(id), CanMessageType::startMotor);
        EXPECT_EQ(ExtractCanNodeId(id), 42);
    }

    TEST_F(CanProtocolHandlerTest, TorqueSetpoint_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setTorqueSetpoint);

        EXPECT_CALL(observerMock, OnTorqueSetpoint(_, _)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1, 0x00 }));
    }

    TEST_F(CanProtocolHandlerTest, SetPolePairs_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorParameters, CanMessageType::setPolePairs);

        EXPECT_CALL(observerMock, OnPolePairsChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, SystemParam_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::systemParameters, CanMessageType::setSupplyVoltage);

        EXPECT_CALL(observerMock, OnSupplyVoltageChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, StartListening_RegistersCallback)
    {
        EXPECT_CALL(canMock, ReceiveData(_));
        handler.StartListening();
    }

    TEST_F(CanProtocolHandlerTest, UnknownMotorControlCommand_AcksUnknown)
    {
        auto id = MakeCommandId(CanCategory::motorControl, static_cast<CanMessageType>(0xFF));

        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, UnknownPidCommand_AcksUnknown)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, static_cast<CanMessageType>(0xFF));
        hal::Can::Message msg;
        msg.push_back(2);
        WriteInt16(msg, 1, 100);
        WriteInt16(msg, 3, 100);
        WriteInt16(msg, 5, 100);

        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, UnknownMotorParam_AcksUnknown)
    {
        auto id = MakeCommandId(CanCategory::motorParameters, static_cast<CanMessageType>(0xFF));

        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 3 }));
    }

    TEST_F(CanProtocolHandlerTest, UnknownSystemParam_AcksUnknown)
    {
        auto id = MakeCommandId(CanCategory::systemParameters, static_cast<CanMessageType>(0xFF));
        hal::Can::Message msg;
        msg.push_back(4);
        WriteInt16(msg, 1, 100);

        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, msg);
    }

    TEST_F(CanProtocolHandlerTest, SetControlMode_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setControlMode);

        EXPECT_CALL(observerMock, OnControlModeChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, SpeedSetpoint_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setSpeedSetpoint);

        EXPECT_CALL(observerMock, OnSpeedSetpoint(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1, 0x00 }));
    }

    TEST_F(CanProtocolHandlerTest, PositionSetpoint_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setPositionSetpoint);

        EXPECT_CALL(observerMock, OnPositionSetpoint(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1, 0x00 }));
    }

    TEST_F(CanProtocolHandlerTest, Resistance_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorParameters, CanMessageType::setResistance);

        EXPECT_CALL(observerMock, OnResistanceChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 2 }));
    }

    TEST_F(CanProtocolHandlerTest, Inductance_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorParameters, CanMessageType::setInductance);

        EXPECT_CALL(observerMock, OnInductanceChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 3 }));
    }

    TEST_F(CanProtocolHandlerTest, FluxLinkage_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorParameters, CanMessageType::setFluxLinkage);

        EXPECT_CALL(observerMock, OnFluxLinkageChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 4 }));
    }

    TEST_F(CanProtocolHandlerTest, StartListening_CallbackDispatchesMessage)
    {
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> capturedCallback;

        EXPECT_CALL(canMock, ReceiveData(_)).WillOnce([&capturedCallback](const auto& callback)
            {
                capturedCallback = callback;
            });

        handler.StartListening();

        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(observerMock, OnEmergencyStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        capturedCallback(id, MakeMessage({ 1 }));
    }

    TEST(CanFrameCodecTest, FloatToFixed16_SaturatesAtMax)
    {
        auto result = CanFrameCodec::FloatToFixed16(100.0f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int16_t>::max());
    }

    TEST(CanFrameCodecTest, FloatToFixed16_SaturatesAtMin)
    {
        auto result = CanFrameCodec::FloatToFixed16(-100.0f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int16_t>::min());
    }

    TEST(CanFrameCodecTest, FloatToFixed32_NormalValue)
    {
        auto result = CanFrameCodec::FloatToFixed32(1.5f, 1000);
        EXPECT_EQ(result, 1500);
    }

    TEST(CanFrameCodecTest, FloatToFixed32_SaturatesAtMax)
    {
        auto result = CanFrameCodec::FloatToFixed32(1e15f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int32_t>::max());
    }

    TEST(CanFrameCodecTest, FloatToFixed32_SaturatesAtMin)
    {
        auto result = CanFrameCodec::FloatToFixed32(-1e15f, 1000);
        EXPECT_EQ(result, std::numeric_limits<int32_t>::min());
    }

    TEST(CanFrameCodecTest, Fixed32ToFloat_RoundTrip)
    {
        int32_t fixed = CanFrameCodec::FloatToFixed32(3.14f, 10000);
        float result = CanFrameCodec::Fixed32ToFloat(fixed, 10000);
        EXPECT_NEAR(result, 3.14f, 0.001f);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt32_RoundTrip)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt32(msg, 0, 123456);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, 0), 123456);
    }

    TEST(CanFrameCodecTest, WriteAndReadInt32_NegativeValue)
    {
        hal::Can::Message msg;
        CanFrameCodec::WriteInt32(msg, 0, -987654);
        EXPECT_EQ(CanFrameCodec::ReadInt32(msg, 0), -987654);
    }

    TEST(CanFrameCodecTest, FloatToFixed16_RoundsToNearest)
    {
        auto result = CanFrameCodec::FloatToFixed16(1.2345f, 1000);
        EXPECT_EQ(result, 1235);
    }

    TEST(CanFrameCodecTest, FloatToFixed32_RoundsToNearest)
    {
        auto result = CanFrameCodec::FloatToFixed32(1.2345f, 1000);
        EXPECT_EQ(result, 1235);
    }

    TEST_F(CanProtocolHandlerTest, UnknownSystemCommand_AcksUnknown)
    {
        auto id = MakeSystemId(static_cast<CanMessageType>(0xFF));

        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolHandlerTest, SequenceBypass_OnlyAppliesToMotorControlEmergencyStop)
    {
        auto id1 = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart());
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id1, MakeMessage({ 1 }));

        auto pidId = MakeCommandId(CanCategory::pidTuning, CanMessageType::setSpeedPid);
        hal::Can::Message msg;
        msg.push_back(1);
        for (int i = 0; i < 6; ++i)
            msg.push_back(0);

        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(pidId, msg);
    }

    TEST_F(CanProtocolHandlerTest, EmptyPayload_SequenceProtectedCommand_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({}));
    }

    TEST_F(CanProtocolHandlerTest, EmptyPayload_EmergencyStop_StillProcessed)
    {
        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(observerMock, OnEmergencyStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        handler.ProcessReceivedMessage(id, MakeMessage({}));
    }

}
