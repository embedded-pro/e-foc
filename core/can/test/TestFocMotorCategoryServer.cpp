#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "infra/util/Function.hpp"
#include <bit>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <optional>

namespace
{
    using namespace testing;

    struct AcknowledgerSpy
        : services::CanCommandAcknowledger
    {
        struct Entry
        {
            uint8_t category{};
            uint8_t commandType{};
            services::CanAckStatus status{};
        };

        std::optional<Entry> last;

        void SendCommandAck(uint8_t category, uint8_t commandType, services::CanAckStatus status) override
        {
            last = Entry{ category, commandType, status };
        }

        void Reset()
        {
            last.reset();
        }
    };

    class MockServerObserver
        : public can::FocMotorCategoryServerObserver
    {
    public:
        using can::FocMotorCategoryServerObserver::FocMotorCategoryServerObserver;

        MOCK_METHOD(void, OnStart, (const infra::Function<void(services::CanAckStatus)>&), (override));
        MOCK_METHOD(void, OnStop, (const infra::Function<void(services::CanAckStatus)>&), (override));
        MOCK_METHOD(void, OnClearFault, (const infra::Function<void(services::CanAckStatus)>&), (override));
        MOCK_METHOD(void, OnEmergencyStop, (const infra::Function<void(services::CanAckStatus)>&), (override));
        MOCK_METHOD(void, OnSelectControlMode, (can::FocMotorMode, (const infra::Function<void(can::FocMotorMode)>&)), (override));
        MOCK_METHOD(void, OnSetTorqueSetpoint, (foc::Ampere, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetSpeedSetpoint, (foc::RadiansPerSecond, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetPositionSetpoint, (foc::Radians, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetPidCurrent, (float, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetPidSpeed, (float, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSetPidPosition, (float, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnAlign, ((const infra::Function<void(services::CanAckStatus)>&)), (override));
        MOCK_METHOD(void, OnIdentifyElectrical, (const infra::Function<void()>&), (override));
        MOCK_METHOD(void, OnRequestTelemetry, (const infra::Function<void()>&), (override));
        MOCK_METHOD(void, OnSetEncoderResolution, (uint32_t, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnConfigureTelemetryRate, (uint32_t, (const infra::Function<void()>&)), (override));
        MOCK_METHOD(void, OnSelectAlgorithm, (can::FocControlLoop, uint8_t, (const infra::Function<void(const can::FocAlgorithms&)>&)), (override));
        MOCK_METHOD(void, OnQueryAlgorithms, ((const infra::Function<void(const can::FocAlgorithms&)>&)), (override));
        MOCK_METHOD(void, OnSetParameter, (can::FocParameter, float, (const infra::Function<void(can::FocParameterValue)>&)), (override));
        MOCK_METHOD(void, OnQueryParameter, (can::FocParameter, (const infra::Function<void(can::FocParameterValue)>&)), (override));
        MOCK_METHOD(void, OnCalibrate, ((const infra::Function<void(services::CanAckStatus)>&)), (override));
        MOCK_METHOD(void, OnClearCalibration, ((const infra::Function<void(services::CanAckStatus)>&)), (override));
        MOCK_METHOD(void, OnClearDiagnostics, ((const infra::Function<void()>&)), (override));
    };

    class FocMotorCategoryServerTest
        : public Test
    {
    public:
        FocMotorCategoryServerTest()
        {
            EXPECT_CALL(canMock, SendData(_, _, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                    {
                        lastSentMsgType = services::ExtractCanMessageType(id.Get29BitId());
                        lastSentMsg = msg;
                        if (lastSentMsgType == services::canCategoryErrorResponseMessageTypeId && msg.size() >= 2)
                        {
                            categoryErrorSent = true;
                            lastCategoryErrorOriginCmd = msg[0];
                            lastCategoryError = static_cast<can::FocMotorCategoryError>(msg[1]);
                        }
                        cb(true);
                    }));
            server.SetAcknowledger(ackSpy);
        }

        hal::Can::Message MakePayload(uint8_t seqByte) const
        {
            hal::Can::Message msg;
            msg.push_back(seqByte);
            return msg;
        }

        hal::Can::Message MakeSetpointPayload(int16_t wireValue) const
        {
            hal::Can::Message msg;
            msg.resize(3, 0);
            services::CanFrameCodec::WriteInt16(msg, 1, wireValue);
            return msg;
        }

        hal::Can::Message MakeModePayload(can::FocMotorMode mode) const
        {
            hal::Can::Message msg;
            msg.resize(2, 0);
            msg[1] = static_cast<uint8_t>(mode);
            return msg;
        }

        hal::Can::Message MakeUInt32Payload(uint32_t value) const
        {
            hal::Can::Message msg;
            msg.resize(5, 0);
            services::CanFrameCodec::WriteUInt32(msg, 1, value);
            return msg;
        }

        hal::Can::Message MakeParameterPayload(uint8_t parameter, float value) const
        {
            hal::Can::Message msg;
            msg.resize(6, 0);
            msg[1] = parameter;
            services::CanFrameCodec::WriteUInt32(msg, 2, std::bit_cast<uint32_t>(value));
            return msg;
        }

        hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes) const
        {
            hal::Can::Message msg;
            for (const auto byte : bytes)
                msg.push_back(byte);
            return msg;
        }

        StrictMock<hal::CanMock> canMock;
        services::CanFrameTransport transport{ canMock, 1 };
        can::FocMotorCategoryServer server{ transport };
        AcknowledgerSpy ackSpy;
        StrictMock<MockServerObserver> observer{ server };

        uint8_t lastSentMsgType{};
        hal::Can::Message lastSentMsg;
        bool categoryErrorSent{ false };
        uint8_t lastCategoryErrorOriginCmd{};
        can::FocMotorCategoryError lastCategoryError{ can::FocMotorCategoryError::busy };
    };
}

TEST_F(FocMotorCategoryServerTest, CategoryId_IsFocMotorCategoryId)
{
    EXPECT_EQ(server.Id(), can::focMotorCategoryId);
}

TEST_F(FocMotorCategoryServerTest, HandleStart_InvokesOnStartObserver)
{
    EXPECT_CALL(observer, OnStart(_))
        .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& ack)
            {
                ack(services::CanAckStatus::success);
            }));

    server.HandleMessage(can::focStartId, MakePayload(1));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focStartId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleStop_InvokesOnStopObserver)
{
    EXPECT_CALL(observer, OnStop(_))
        .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& ack)
            {
                ack(services::CanAckStatus::success);
            }));

    server.HandleMessage(can::focStopId, MakePayload(1));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleSetTorqueSetpoint_DecodesCurrentCorrectly)
{
    foc::Ampere received{ 0.0f };
    EXPECT_CALL(observer, OnSetTorqueSetpoint(_, _))
        .WillOnce(Invoke([&received](foc::Ampere val, const infra::Function<void()>& done)
            {
                received = val;
                done();
            }));

    server.HandleMessage(can::focSetTorqueSetpointId, MakeSetpointPayload(15));

    EXPECT_NEAR(received.Value(), 15.0f / can::focCurrentScale, 0.001f);
}

TEST_F(FocMotorCategoryServerTest, HandleSetSpeedSetpoint_DecodesSpeedCorrectly)
{
    foc::RadiansPerSecond received{ 0.0f };
    EXPECT_CALL(observer, OnSetSpeedSetpoint(_, _))
        .WillOnce(Invoke([&received](foc::RadiansPerSecond val, const infra::Function<void()>& done)
            {
                received = val;
                done();
            }));

    server.HandleMessage(can::focSetSpeedSetpointId, MakeSetpointPayload(300));

    EXPECT_NEAR(received.Value(), 300.0f / can::focSpeedScale, 0.01f);
}

TEST_F(FocMotorCategoryServerTest, HandleSetPositionSetpoint_DecodesPositionCorrectly)
{
    foc::Radians received{ 0.0f };
    EXPECT_CALL(observer, OnSetPositionSetpoint(_, _))
        .WillOnce(Invoke([&received](foc::Radians val, const infra::Function<void()>& done)
            {
                received = val;
                done();
            }));

    server.HandleMessage(can::focSetPositionSetpointId, MakeSetpointPayload(314));

    EXPECT_NEAR(received.Value(), 314.0f / can::focPositionScale, 0.001f);
}

TEST_F(FocMotorCategoryServerTest, HandleSelectControlMode_ValidMode_InvokesObserver)
{
    can::FocMotorMode received{ can::FocMotorMode::torque };
    EXPECT_CALL(observer, OnSelectControlMode(_, _))
        .WillOnce(Invoke([&received](can::FocMotorMode mode, const infra::Function<void(can::FocMotorMode)>& cb)
            {
                received = mode;
                cb(mode);
            }));

    server.HandleMessage(can::focSelectControlModeId, MakeModePayload(can::FocMotorMode::speed));

    EXPECT_EQ(received, can::FocMotorMode::speed);
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    EXPECT_EQ(lastSentMsgType, can::focSelectControlModeResponseId);
}

TEST_F(FocMotorCategoryServerTest, HandleSelectControlMode_InvalidMode_IsRejected)
{
    hal::Can::Message msg;
    msg.resize(2, 0);
    msg[1] = 0xFF;
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSelectControlModeId, msg));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, SendCategoryError_EmitsCategoryErrorFrame)
{
    server.SendCategoryError(can::focSetPidCurrentId, can::FocMotorCategoryError::applicationError);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focSetPidCurrentId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::applicationError);
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::categoryError);
}

TEST_F(FocMotorCategoryServerTest, HandleSetPidCurrent_ParsesBandwidthAndInvokesObserver)
{
    EXPECT_CALL(observer, OnSetPidCurrent(_, _))
        .WillOnce(Invoke([](float, const infra::Function<void()>& done)
            {
                done();
            }));

    server.HandleMessage(can::focSetPidCurrentId, MakeSetpointPayload(1000));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleClearFault_InvokesOnClearFaultObserver)
{
    EXPECT_CALL(observer, OnClearFault(_))
        .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& ack)
            {
                ack(services::CanAckStatus::success);
            }));

    server.HandleMessage(can::focClearFaultId, MakePayload(1));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleEmergencyStop_InvokesOnEmergencyStopObserver)
{
    EXPECT_CALL(observer, OnEmergencyStop(_))
        .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& ack)
            {
                ack(services::CanAckStatus::invalidState);
            }));

    server.HandleMessage(can::focEmergencyStopId, MakePayload(1));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCategoryServerTest, HandleSetTorqueSetpoint_ShortPayload_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetTorqueSetpointId, MakePayload(1)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleSetSpeedSetpoint_ShortPayload_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetSpeedSetpointId, MakePayload(1)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleSetPositionSetpoint_ShortPayload_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetPositionSetpointId, MakePayload(1)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleIdentifyElectrical_InvokesObserver)
{
    EXPECT_CALL(observer, OnIdentifyElectrical(_))
        .WillOnce(Invoke([](const infra::Function<void()>& done)
            {
                done();
            }));

    server.HandleMessage(can::focIdentifyElectricalId, MakePayload(1));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleRequestTelemetry_InvokesObserver)
{
    EXPECT_CALL(observer, OnRequestTelemetry(_))
        .WillOnce(Invoke([](const infra::Function<void()>& done)
            {
                done();
            }));

    server.HandleMessage(can::focRequestTelemetryId, MakePayload(1));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleSetPidSpeed_ParsesBandwidthAndInvokesObserver)
{
    EXPECT_CALL(observer, OnSetPidSpeed(_, _))
        .WillOnce(Invoke([](float, const infra::Function<void()>& done)
            {
                done();
            }));

    server.HandleMessage(can::focSetPidSpeedId, MakeSetpointPayload(100));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleSetPidPosition_ParsesBandwidthAndInvokesObserver)
{
    EXPECT_CALL(observer, OnSetPidPosition(_, _))
        .WillOnce(Invoke([](float, const infra::Function<void()>& done)
            {
                done();
            }));

    server.HandleMessage(can::focSetPidPositionId, MakeSetpointPayload(18));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleSetPidCurrent_ShortPayload_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetPidCurrentId, MakePayload(1)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleSetPidSpeed_ShortPayload_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetPidSpeedId, MakePayload(1)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleSetPidPosition_ShortPayload_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetPidPositionId, MakePayload(1)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleSetEncoderResolution_ShortPayload_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetEncoderResolutionId, MakePayload(1)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleConfigureTelemetryRate_ShortPayload_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focConfigureTelemetryRateId, MakePayload(1)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleSetEncoderResolution_ParsesResolutionAndInvokesObserver)
{
    EXPECT_CALL(observer, OnSetEncoderResolution(4000u, _))
        .WillOnce(Invoke([](uint32_t, const infra::Function<void()>& done)
            {
                done();
            }));

    server.HandleMessage(can::focSetEncoderResolutionId, MakeUInt32Payload(4000));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleConfigureTelemetryRate_ParsesRateAndInvokesObserver)
{
    EXPECT_CALL(observer, OnConfigureTelemetryRate(100u, _))
        .WillOnce(Invoke([](uint32_t, const infra::Function<void()>& done)
            {
                done();
            }));

    server.HandleMessage(can::focConfigureTelemetryRateId, MakeUInt32Payload(100));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleSelectAlgorithm_AnswersWithTheModeAndEveryLoopsAlgorithm)
{
    EXPECT_CALL(observer, OnSelectAlgorithm(can::FocControlLoop::speed, static_cast<uint8_t>(can::FocSpeedAlgorithm::adrc), _))
        .WillOnce(Invoke([](can::FocControlLoop, uint8_t, const infra::Function<void(const can::FocAlgorithms&)>& onDone)
            {
                onDone(can::FocAlgorithms{ can::FocMotorMode::speed, can::FocCurrentAlgorithm::decoupledPid, can::FocSpeedAlgorithm::adrc, can::FocPositionAlgorithm::pid });
            }));

    server.HandleMessage(can::focSelectAlgorithmId, MakeMessage({ 0x01, 0x01, 0x02 }));

    EXPECT_EQ(lastSentMsgType, can::focAlgorithmsResponseId);
    EXPECT_EQ(lastSentMsg, MakeMessage({ 0x01, 0x01, 0x02, 0x00 }));
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focSelectAlgorithmId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleSelectAlgorithm_LastAlgorithmOfEachLoop_IsAccepted)
{
    EXPECT_CALL(observer, OnSelectAlgorithm(can::FocControlLoop::current, 3, _));
    EXPECT_CALL(observer, OnSelectAlgorithm(can::FocControlLoop::speed, 3, _));
    EXPECT_CALL(observer, OnSelectAlgorithm(can::FocControlLoop::position, 4, _));

    EXPECT_EQ(services::CanDispatchResult::handled, server.HandleMessage(can::focSelectAlgorithmId, MakeMessage({ 0x01, 0x00, 0x03 })));
    EXPECT_EQ(services::CanDispatchResult::handled, server.HandleMessage(can::focSelectAlgorithmId, MakeMessage({ 0x01, 0x01, 0x03 })));
    EXPECT_EQ(services::CanDispatchResult::handled, server.HandleMessage(can::focSelectAlgorithmId, MakeMessage({ 0x01, 0x02, 0x04 })));
}

TEST_F(FocMotorCategoryServerTest, HandleSelectAlgorithm_UnknownLoopOrAlgorithm_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSelectAlgorithmId, MakeMessage({ 0x01, 0x03, 0x00 })));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSelectAlgorithmId, MakeMessage({ 0x01, 0x00, 0x04 })));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSelectAlgorithmId, MakeMessage({ 0x01, 0x01, 0x04 })));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSelectAlgorithmId, MakeMessage({ 0x01, 0x02, 0x05 })));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleQueryAlgorithms_AnswersWithTheAlgorithms)
{
    EXPECT_CALL(observer, OnQueryAlgorithms(_))
        .WillOnce(Invoke([](const infra::Function<void(const can::FocAlgorithms&)>& onDone)
            {
                onDone(can::FocAlgorithms{ can::FocMotorMode::position, can::FocCurrentAlgorithm::pid, can::FocSpeedAlgorithm::lqi, can::FocPositionAlgorithm::twoDof });
            }));

    server.HandleMessage(can::focQueryAlgorithmsId, MakePayload(1));

    EXPECT_EQ(lastSentMsgType, can::focAlgorithmsResponseId);
    EXPECT_EQ(lastSentMsg, MakeMessage({ 0x02, 0x00, 0x01, 0x04 }));
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focQueryAlgorithmsId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleSetParameter_DecodesTheValueAndAnswersWithTheValueHeld)
{
    EXPECT_CALL(observer, OnSetParameter(can::FocParameter::speedObserverBandwidthRatio, 8.0f, _))
        .WillOnce(Invoke([](can::FocParameter, float value, const infra::Function<void(can::FocParameterValue)>& onDone)
            {
                onDone(can::FocParameterValue{ value, false });
            }));

    server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0x13, 8.0f));

    EXPECT_EQ(lastSentMsgType, can::focParameterResponseId);
    EXPECT_EQ(lastSentMsg, MakeMessage({ 0x13, 0x00, 0x41, 0x00, 0x00, 0x00 }));
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focSetParameterId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleSetParameter_ZeroOnAnAutomaticParameter_ReachesTheObserver)
{
    EXPECT_CALL(observer, OnSetParameter(can::FocParameter::speedBandwidth, 0.0f, _));

    EXPECT_EQ(services::CanDispatchResult::handled, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0x10, 0.0f)));
}

TEST_F(FocMotorCategoryServerTest, HandleSetParameter_ValueOutsideTheParameterDomain_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0x13, 25.0f)));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0x11, 0.0f)));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0x40, std::numeric_limits<float>::quiet_NaN())));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0x42, 4.5f)));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0x62, 300000.0f)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleSetParameter_ReadOnlyParameter_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0x80, 0.0f)));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0xA0, 1.0f)));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0xB0, 0.0f)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleSetParameter_UnknownParameter_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focSetParameterId, MakeParameterPayload(0x05, 1.0f)));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleQueryParameter_AnswersWithTheValueAndTheAutomaticFlag)
{
    EXPECT_CALL(observer, OnQueryParameter(can::FocParameter::currentBandwidth, _))
        .WillOnce(Invoke([](can::FocParameter, const infra::Function<void(can::FocParameterValue)>& onDone)
            {
                onDone(can::FocParameterValue{ 1000.0f, true });
            }));

    server.HandleMessage(can::focQueryParameterId, MakeMessage({ 0x01, 0x00 }));

    EXPECT_EQ(lastSentMsgType, can::focParameterResponseId);
    EXPECT_EQ(lastSentMsg, MakeMessage({ 0x00, 0x01, 0x44, 0x7A, 0x00, 0x00 }));
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focQueryParameterId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleQueryParameter_UnknownParameter_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focQueryParameterId, MakeMessage({ 0x01, 0x30 })));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, HandleQueryParameter_ReadOnlyParameter_AnswersWithTheReading)
{
    EXPECT_CALL(observer, OnQueryParameter(can::FocParameter::resetCause, _))
        .WillOnce(Invoke([](can::FocParameter, const infra::Function<void(can::FocParameterValue)>& onDone)
            {
                onDone(can::FocParameterValue{ 4.0f, false });
            }));

    server.HandleMessage(can::focQueryParameterId, MakeMessage({ 0x01, 0xB0 }));

    EXPECT_EQ(lastSentMsgType, can::focParameterResponseId);
    EXPECT_EQ(lastSentMsg, MakeMessage({ 0xB0, 0x00, 0x40, 0x80, 0x00, 0x00 }));
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleCalibrate_AnswersWithTheStatusTheObserverReports)
{
    EXPECT_CALL(observer, OnCalibrate(_))
        .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& onDone)
            {
                onDone(services::CanAckStatus::invalidState);
            }));

    server.HandleMessage(can::focCalibrateId, MakePayload(1));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focCalibrateId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCategoryServerTest, HandleClearCalibration_AnswersWithTheStatusTheObserverReports)
{
    EXPECT_CALL(observer, OnClearCalibration(_))
        .WillOnce(Invoke([](const infra::Function<void(services::CanAckStatus)>& onDone)
            {
                onDone(services::CanAckStatus::success);
            }));

    server.HandleMessage(can::focClearCalibrationId, MakePayload(1));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focClearCalibrationId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, HandleClearDiagnostics_AcknowledgesOnceTheObserverIsDone)
{
    EXPECT_CALL(observer, OnClearDiagnostics(_))
        .WillOnce(Invoke([](const infra::Function<void()>& onDone)
            {
                onDone();
            }));

    server.HandleMessage(can::focClearDiagnosticsId, MakePayload(1));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focClearDiagnosticsId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCategoryServerTest, MaintenanceCommands_WithAPayload_AreRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focCalibrateId, MakeMessage({ 0x01, 0x00 })));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focClearCalibrationId, MakeMessage({ 0x01, 0x00 })));
    EXPECT_EQ(services::CanDispatchResult::rejected, server.HandleMessage(can::focClearDiagnosticsId, MakeMessage({ 0x01, 0x00 })));
    EXPECT_FALSE(ackSpy.last.has_value());
}

TEST_F(FocMotorCategoryServerTest, BroadcastTelemetryCurrents_SendsTheSampleAtTelemetryPriority)
{
    std::optional<uint32_t> rawId;
    EXPECT_CALL(canMock, SendData(_, _, _))
        .WillOnce(Invoke([&](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
            {
                rawId = id.Get29BitId();
                lastSentMsg = msg;
                cb(true);
            }));

    bool sent{ false };
    EXPECT_TRUE(server.BroadcastTelemetryCurrents(0x2A, foc::Ampere{ 0.512f }, foc::Ampere{ -1.0f }, foc::Radians{ -3.1416f }, [&sent](bool success)
        {
            sent = success;
        }));

    ASSERT_TRUE(rawId.has_value());
    EXPECT_EQ(services::ExtractCanMessageType(*rawId), can::focTelemetryCurrentsId);
    EXPECT_EQ(services::ExtractCanPriority(*rawId), services::CanPriority::telemetry);
    EXPECT_EQ(lastSentMsg, MakeMessage({ 0x2A, 0x02, 0x00, 0xFC, 0x18, 0x85, 0x48 }));
    EXPECT_TRUE(sent);
}

TEST_F(FocMotorCategoryServerTest, BroadcastTelemetryCurrents_SaturatesCurrentsBeyondTheWireRange)
{
    EXPECT_TRUE(server.BroadcastTelemetryCurrents(0, foc::Ampere{ 40.0f }, foc::Ampere{ -40.0f }, foc::Radians{ 0.0f }, [](bool) {}));

    EXPECT_EQ(lastSentMsg, MakeMessage({ 0x00, 0x7F, 0xFF, 0x80, 0x00, 0x00, 0x00 }));
}
