#include "source/services/can_protocol/core/test/CanMock.hpp"
#include "source/services/can_protocol/server/CanCategoryHandlers.hpp"
#include "source/services/can_protocol/server/CanProtocolServerImpl.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace testing;
    using namespace services;

    class CanProtocolServerObserverMock
        : public CanProtocolServerObserver
    {
    public:
        using CanProtocolServerObserver::CanProtocolServerObserver;

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

        MOCK_METHOD(void, OnSupplyVoltageChanged, (float voltageVolts), (override));
        MOCK_METHOD(void, OnMaxCurrentChanged, (float currentAmps), (override));

        MOCK_METHOD(void, OnDataRequested, (DataRequestFlags flags, DataResponse& response), (override));
    };

    class CanProtocolServerTest
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

        CanProtocolServer::Config config{ 1, 500 };
        StrictMock<hal::CanMock> canMock;
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receiveCallback;
        FixtureInit fixtureInit{ canMock, receiveCallback };

        CanProtocolServerImpl::Handlers handlers;
        CanProtocolServerImpl server{ canMock, config, handlers };

        CanMotorControlHandler motorControlHandler{ server };
        CanPidTuningHandler pidTuningHandler{ server };
        CanSystemParametersHandler systemParametersHandler{ server };
        CanSystemHandler systemHandler{ server };

        StrictMock<CanProtocolServerObserverMock> observerMock{ server };

        void SetUp() override
        {
            handlers.push_back(&motorControlHandler);
            handlers.push_back(&pidTuningHandler);
            handlers.push_back(&systemParametersHandler);
            handlers.push_back(&systemHandler);
        }
    };

    TEST_F(CanProtocolServerTest, StartMotor_NotifiesObserver)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart());
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, StopMotor_NotifiesObserver)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::stopMotor);

        EXPECT_CALL(observerMock, OnMotorStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, EmergencyStop_NotifiesObserverWithoutSequenceCheck)
    {
        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(observerMock, OnEmergencyStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 99 }));
    }

    TEST_F(CanProtocolServerTest, SetControlMode_Speed)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setControlMode);

        EXPECT_CALL(observerMock, OnControlModeChanged(CanControlMode::speed));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1, static_cast<uint8_t>(CanControlMode::speed) }));
    }

    TEST_F(CanProtocolServerTest, SetControlMode_InvalidMode_RejectsWithInvalidPayload)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setControlMode);

        EXPECT_CALL(observerMock, OnControlModeChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1, 0xFF }));
    }

    TEST_F(CanProtocolServerTest, SetTorqueSetpoint_DecodesFixedPoint)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setTorqueSetpoint);
        hal::Can::Message msg;
        msg.push_back(1);
        WriteInt16(msg, 1, 0);
        WriteInt16(msg, 3, 1500);

        EXPECT_CALL(observerMock, OnTorqueSetpoint(FloatNear(0.0f, 0.01f), FloatNear(1.5f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, SetSpeedSetpoint_DecodesFixedPoint)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setSpeedSetpoint);
        hal::Can::Message msg;
        msg.push_back(1);
        WriteInt32(msg, 1, 10000);

        EXPECT_CALL(observerMock, OnSpeedSetpoint(FloatNear(10.0f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, SetPositionSetpoint_DecodesFixedPoint)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setPositionSetpoint);
        hal::Can::Message msg;
        msg.push_back(1);
        WriteInt32(msg, 1, 31416);

        EXPECT_CALL(observerMock, OnPositionSetpoint(FloatNear(3.1416f, 0.001f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, SetCurrentIdPid_DecodesGains)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setCurrentIdPid);
        hal::Can::Message msg;
        msg.push_back(1);
        WriteInt16(msg, 1, 1200);
        WriteInt16(msg, 3, 500);
        WriteInt16(msg, 5, 100);

        EXPECT_CALL(observerMock, OnCurrentIdPidChanged(FloatNear(1.2f, 0.01f), FloatNear(0.5f, 0.01f), FloatNear(0.1f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, SetCurrentIqPid_DecodesGains)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setCurrentIqPid);
        hal::Can::Message msg;
        msg.push_back(2);
        WriteInt16(msg, 1, 3000);
        WriteInt16(msg, 3, 100);
        WriteInt16(msg, 5, 50);

        EXPECT_CALL(observerMock, OnCurrentIqPidChanged(FloatNear(3.0f, 0.01f), FloatNear(0.1f, 0.01f), FloatNear(0.05f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, SetSpeedPid_DecodesGains)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setSpeedPid);
        hal::Can::Message msg;
        msg.push_back(3);
        WriteInt16(msg, 1, 2000);
        WriteInt16(msg, 3, 300);
        WriteInt16(msg, 5, 0);

        EXPECT_CALL(observerMock, OnSpeedPidChanged(FloatNear(2.0f, 0.01f), FloatNear(0.3f, 0.01f), FloatNear(0.0f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, SetPositionPid_DecodesGains)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setPositionPid);
        hal::Can::Message msg;
        msg.push_back(4);
        WriteInt16(msg, 1, 5000);
        WriteInt16(msg, 3, 1000);
        WriteInt16(msg, 5, 200);

        EXPECT_CALL(observerMock, OnPositionPidChanged(FloatNear(5.0f, 0.01f), FloatNear(1.0f, 0.01f), FloatNear(0.2f, 0.01f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, PidCommand_TooShortPayload_RejectsWithInvalidPayload)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, CanMessageType::setCurrentIdPid);

        EXPECT_CALL(observerMock, OnCurrentIdPidChanged(_, _, _)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1, 0x01 }));
    }

    TEST_F(CanProtocolServerTest, SetSupplyVoltage)
    {
        auto id = MakeCommandId(CanCategory::systemParameters, CanMessageType::setSupplyVoltage);
        hal::Can::Message msg;
        msg.push_back(5);
        WriteInt16(msg, 1, 2400);

        EXPECT_CALL(observerMock, OnSupplyVoltageChanged(FloatNear(24.0f, 0.1f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, SetMaxCurrent)
    {
        auto id = MakeCommandId(CanCategory::systemParameters, CanMessageType::setMaxCurrent);
        hal::Can::Message msg;
        msg.push_back(6);
        WriteInt16(msg, 1, 10000);

        EXPECT_CALL(observerMock, OnMaxCurrentChanged(FloatNear(10.0f, 0.1f)));
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, HeartbeatReceived_SilentlyHandled)
    {
        auto id = MakeSystemId(CanMessageType::heartbeat);

        SimulateRx(id, MakeMessage({ canProtocolVersion }));
    }

    TEST_F(CanProtocolServerTest, RequestData_NotifiesObserverAndSendsTelemetry)
    {
        auto id = MakeSystemId(CanMessageType::requestData);

        EXPECT_CALL(observerMock, OnDataRequested(DataRequestFlags::motorStatus, _))
            .WillOnce([](DataRequestFlags, DataResponse& response)
                {
                    response.motorState = CanMotorState::running;
                    response.controlMode = CanControlMode::speed;
                    response.faultCode = CanFaultCode::none;
                });
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ static_cast<uint8_t>(DataRequestFlags::motorStatus) }));
    }

    TEST_F(CanProtocolServerTest, RequestData_AllFlags_SendsAllTelemetry)
    {
        auto id = MakeSystemId(CanMessageType::requestData);

        EXPECT_CALL(observerMock, OnDataRequested(DataRequestFlags::all, _));
        EXPECT_CALL(canMock, SendData(_, _, _)).Times(5);

        SimulateRx(id, MakeMessage({ static_cast<uint8_t>(DataRequestFlags::all) }));
    }

    TEST_F(CanProtocolServerTest, RequestData_EmptyPayload_DefaultsToAll)
    {
        auto id = MakeSystemId(CanMessageType::requestData);

        EXPECT_CALL(observerMock, OnDataRequested(DataRequestFlags::all, _));
        EXPECT_CALL(canMock, SendData(_, _, _)).Times(5);

        SimulateRx(id, MakeMessage({}));
    }

    TEST_F(CanProtocolServerTest, RejectsMessageForDifferentNode)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor, 99);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(0);

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, AcceptsBroadcastMessage)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::emergencyStop, canBroadcastNodeId);

        EXPECT_CALL(observerMock, OnEmergencyStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, Rejects11BitId)
    {
        auto id = hal::Can::Id::Create11BitId(0x100);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(0);

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, RateLimiting_RejectsExcessMessages)
    {
        CanProtocolServer::Config limitedConfig{ 1, 3 };
        StrictMock<hal::CanMock> limitedCan;

        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> limitedReceiveCallback;

        EXPECT_CALL(limitedCan, ReceiveData(_)).WillOnce([&limitedReceiveCallback](const auto& callback)
            {
                limitedReceiveCallback = callback;
            });
        ON_CALL(limitedCan, SendData(_, _, _))
            .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                {
                    cb(true);
                }));

        CanProtocolServerImpl::Handlers limitedHandlers;
        CanProtocolServerImpl limitedServer(limitedCan, limitedConfig, limitedHandlers);

        CanMotorControlHandler limitedMotorHandler(limitedServer);
        limitedHandlers.push_back(&limitedMotorHandler);

        StrictMock<CanProtocolServerObserverMock> limitedObserver(limitedServer);

        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(limitedObserver, OnEmergencyStop()).Times(3);
        EXPECT_CALL(limitedCan, SendData(_, _, _)).Times(3);

        for (int i = 0; i < 3; ++i)
            limitedReceiveCallback(id, MakeMessage({ 1 }));

        limitedReceiveCallback(id, MakeMessage({ 1 }));

        EXPECT_TRUE(limitedServer.IsRateLimited());
    }

    TEST_F(CanProtocolServerTest, RateLimiting_ResetsCounter)
    {
        CanProtocolServer::Config limitedConfig{ 1, 2 };
        StrictMock<hal::CanMock> limitedCan;

        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> limitedReceiveCallback;

        EXPECT_CALL(limitedCan, ReceiveData(_)).WillOnce([&limitedReceiveCallback](const auto& callback)
            {
                limitedReceiveCallback = callback;
            });
        ON_CALL(limitedCan, SendData(_, _, _))
            .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                {
                    cb(true);
                }));

        CanProtocolServerImpl::Handlers limitedHandlers;
        CanProtocolServerImpl limitedServer(limitedCan, limitedConfig, limitedHandlers);

        CanMotorControlHandler limitedMotorHandler(limitedServer);
        limitedHandlers.push_back(&limitedMotorHandler);

        StrictMock<CanProtocolServerObserverMock> limitedObserver(limitedServer);

        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(limitedObserver, OnEmergencyStop()).Times(2);
        EXPECT_CALL(limitedCan, SendData(_, _, _)).Times(2);

        limitedReceiveCallback(id, MakeMessage({ 1 }));
        limitedReceiveCallback(id, MakeMessage({ 1 }));

        EXPECT_TRUE(limitedServer.IsRateLimited());

        limitedServer.ResetRateCounter();
        EXPECT_FALSE(limitedServer.IsRateLimited());
    }

    TEST_F(CanProtocolServerTest, SequenceValidation_RejectsDuplicate)
    {
        auto id1 = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(1);
        EXPECT_CALL(canMock, SendData(_, _, _)).Times(1);

        SimulateRx(id1, MakeMessage({ 1 }));

        auto id2 = MakeCommandId(CanCategory::motorControl, CanMessageType::stopMotor);

        EXPECT_CALL(canMock, SendData(_, _, _)).Times(1);

        SimulateRx(id2, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, SequenceValidation_AcceptsSequentialMessages)
    {
        auto idStart = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);
        auto idStop = MakeCommandId(CanCategory::motorControl, CanMessageType::stopMotor);

        EXPECT_CALL(observerMock, OnMotorStart());
        EXPECT_CALL(observerMock, OnMotorStop());
        EXPECT_CALL(canMock, SendData(_, _, _)).Times(2);

        SimulateRx(idStart, MakeMessage({ 1 }));
        SimulateRx(idStop, MakeMessage({ 2 }));
    }

    TEST_F(CanProtocolServerTest, SequenceWrapsAround)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(2);
        EXPECT_CALL(canMock, SendData(_, _, _)).Times(2);

        SimulateRx(id, MakeMessage({ 255 }));
        SimulateRx(id, MakeMessage({ 0 }));
    }

    TEST_F(CanProtocolServerTest, UnknownCategory_SilentlyDiscarded)
    {
        uint32_t rawId = MakeCanId(CanPriority::command, static_cast<CanCategory>(0xF), CanMessageType::startMotor, 1);
        auto id = hal::Can::Id::Create29BitId(rawId);

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, SendCommandAck_IncludesCategoryCommandTypeAndStatus)
    {
        EXPECT_CALL(canMock, SendData(_, _, _)).WillOnce([](hal::Can::Id, const hal::Can::Message& data, const auto&)
            {
                ASSERT_GE(data.size(), 3u);
                EXPECT_EQ(data[0], static_cast<uint8_t>(CanCategory::motorControl));
                EXPECT_EQ(data[1], static_cast<uint8_t>(CanMessageType::startMotor));
                EXPECT_EQ(data[2], static_cast<uint8_t>(CanAckStatus::success));
            });

        server.SendCommandAck(CanCategory::motorControl, CanMessageType::startMotor, CanAckStatus::success);
    }

    TEST_F(CanProtocolServerTest, TorqueSetpoint_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setTorqueSetpoint);

        EXPECT_CALL(observerMock, OnTorqueSetpoint(_, _)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1, 0x00 }));
    }

    TEST_F(CanProtocolServerTest, SystemParam_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::systemParameters, CanMessageType::setSupplyVoltage);

        EXPECT_CALL(observerMock, OnSupplyVoltageChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, ConstructorAutoRegistersReceiveCallback)
    {
        StrictMock<hal::CanMock> testCan;

        EXPECT_CALL(testCan, ReceiveData(_));
        ON_CALL(testCan, SendData(_, _, _))
            .WillByDefault(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                {
                    cb(true);
                }));

        CanProtocolServerImpl::Handlers testHandlers;
        CanProtocolServerImpl testServer(testCan, config, testHandlers);
    }

    TEST_F(CanProtocolServerTest, ConstructorCallback_DispatchesMessage)
    {
        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(observerMock, OnEmergencyStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        receiveCallback(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, UnknownMotorControlCommand_AcksUnknown)
    {
        auto id = MakeCommandId(CanCategory::motorControl, static_cast<CanMessageType>(0xFF));

        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, UnknownPidCommand_AcksUnknown)
    {
        auto id = MakeCommandId(CanCategory::pidTuning, static_cast<CanMessageType>(0xFF));
        hal::Can::Message msg;
        msg.push_back(2);
        WriteInt16(msg, 1, 100);
        WriteInt16(msg, 3, 100);
        WriteInt16(msg, 5, 100);

        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, UnknownSystemParam_AcksUnknown)
    {
        auto id = MakeCommandId(CanCategory::systemParameters, static_cast<CanMessageType>(0xFF));
        hal::Can::Message msg;
        msg.push_back(4);
        WriteInt16(msg, 1, 100);

        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, msg);
    }

    TEST_F(CanProtocolServerTest, SetControlMode_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setControlMode);

        EXPECT_CALL(observerMock, OnControlModeChanged(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, SpeedSetpoint_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setSpeedSetpoint);

        EXPECT_CALL(observerMock, OnSpeedSetpoint(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1, 0x00 }));
    }

    TEST_F(CanProtocolServerTest, PositionSetpoint_TooShort_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::setPositionSetpoint);

        EXPECT_CALL(observerMock, OnPositionSetpoint(_)).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1, 0x00 }));
    }

    TEST_F(CanProtocolServerTest, UnknownSystemCommand_AcksUnknown)
    {
        auto id = MakeSystemId(static_cast<CanMessageType>(0xFF));

        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({ 1 }));
    }

    TEST_F(CanProtocolServerTest, SequenceBypass_OnlyAppliesToMotorControlEmergencyStop)
    {
        auto id1 = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart());
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id1, MakeMessage({ 1 }));

        auto pidId = MakeCommandId(CanCategory::pidTuning, CanMessageType::setSpeedPid);
        hal::Can::Message msg;
        msg.push_back(1);
        for (int i = 0; i < 6; ++i)
            msg.push_back(0);

        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(pidId, msg);
    }

    TEST_F(CanProtocolServerTest, EmptyPayload_SequenceProtectedCommand_Rejected)
    {
        auto id = MakeCommandId(CanCategory::motorControl, CanMessageType::startMotor);

        EXPECT_CALL(observerMock, OnMotorStart()).Times(0);
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({}));
    }

    TEST_F(CanProtocolServerTest, EmptyPayload_EmergencyStop_StillProcessed)
    {
        auto id = MakeEmergencyId(CanMessageType::emergencyStop);

        EXPECT_CALL(observerMock, OnEmergencyStop());
        EXPECT_CALL(canMock, SendData(_, _, _));

        SimulateRx(id, MakeMessage({}));
    }
}
