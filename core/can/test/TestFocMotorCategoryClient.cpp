#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceSource.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCategoryClient.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include "infra/util/Function.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <optional>

namespace
{
    using namespace testing;

    struct SequenceSourceStub
        : services::CanSequenceSource
    {
        uint8_t ReserveSequence(uint16_t, uint8_t, uint8_t) override
        {
            return seq++;
        }

        void CancelReservation(uint16_t, uint8_t) override
        {
            --seq;
        }

        void ReturnSequence(uint16_t, uint8_t) override
        {
            --seq;
        }

        uint8_t seq{};
    };

    class MockClientObserver
        : public can::FocMotorCategoryClientObserver
    {
    public:
        using can::FocMotorCategoryClientObserver::FocMotorCategoryClientObserver;

        MOCK_METHOD(void, OnSelectControlModeResponse, (uint16_t sourceNodeId, can::FocMotorMode activeMode), (override));
        MOCK_METHOD(void, OnCategoryError, (uint16_t sourceNodeId, uint8_t originCommandId, can::FocMotorCategoryError errorCode), (override));
        MOCK_METHOD(void, OnTelemetryStatus, (uint16_t sourceNodeId, can::FocMotorState state, can::FocFaultCode fault), (override));
        MOCK_METHOD(void, OnTelemetryMotion, (uint16_t sourceNodeId, foc::RadiansPerSecond speed, foc::Radians position), (override));
        MOCK_METHOD(void, OnTelemetryCurrents, (uint16_t sourceNodeId, uint8_t sequence, foc::Ampere id, foc::Ampere iq, foc::Radians position), (override));
        MOCK_METHOD(void, OnContractVersionResponse, (uint16_t sourceNodeId, uint8_t major, uint8_t minor), (override));
        MOCK_METHOD(void, OnAlgorithmsResponse, (uint16_t sourceNodeId, const can::FocAlgorithms& algorithms), (override));
        MOCK_METHOD(void, OnParameterResponse, (uint16_t sourceNodeId, can::FocParameter parameter, can::FocParameterValue value), (override));
        MOCK_METHOD(void, OnFaultRecordResponse, (uint16_t sourceNodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs), (override));
        MOCK_METHOD(void, OnFaultConditionsResponse, (uint16_t sourceNodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent), (override));
    };

    struct FrameOriginStub
        : can::CanFrameOrigin
    {
        uint16_t SourceNodeId() const override
        {
            return node;
        }

        uint16_t node{ 3 };
    };

    class FocMotorCategoryClientTest
        : public Test
    {
    public:
        FocMotorCategoryClientTest()
        {
            EXPECT_CALL(canMock, SendData(_, _, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                    {
                        lastSentMsgType = services::ExtractCanMessageType(id.Get29BitId());
                        lastSentMsg = msg;
                        cb(true);
                    }));
        }

        hal::Can::Message MakeMessage(std::initializer_list<uint8_t> bytes) const
        {
            hal::Can::Message msg;
            for (const auto byte : bytes)
                msg.push_back(byte);
            return msg;
        }

        StrictMock<hal::CanMock> canMock;
        services::CanFrameTransport transport{ canMock, 0 };
        SequenceSourceStub seqSource;
        FrameOriginStub origin;
        can::FocMotorCategoryClient client{ transport, seqSource, origin };
        StrictMock<MockClientObserver> observer{ client };

        uint8_t lastSentMsgType{};
        hal::Can::Message lastSentMsg;
    };
}

TEST_F(FocMotorCategoryClientTest, CategoryId_IsFocMotorCategoryId)
{
    EXPECT_EQ(client.Id(), can::focMotorCategoryId);
}

TEST_F(FocMotorCategoryClientTest, SendStart_TransmitsStartCommand)
{
    client.SendStart(1);
    EXPECT_EQ(lastSentMsgType, can::focStartId);
}

TEST_F(FocMotorCategoryClientTest, SendStop_TransmitsStopCommand)
{
    client.SendStop(1);
    EXPECT_EQ(lastSentMsgType, can::focStopId);
}

TEST_F(FocMotorCategoryClientTest, SendClearFault_TransmitsClearFaultCommand)
{
    client.SendClearFault(1);
    EXPECT_EQ(lastSentMsgType, can::focClearFaultId);
}

TEST_F(FocMotorCategoryClientTest, SendEmergencyStop_TransmitsEmergencyStopCommand)
{
    client.SendEmergencyStop(1);
    EXPECT_EQ(lastSentMsgType, can::focEmergencyStopId);
}

TEST_F(FocMotorCategoryClientTest, SendSelectControlMode_EncodesMode)
{
    client.SendSelectControlMode(1, can::FocMotorMode::speed);
    EXPECT_EQ(lastSentMsgType, can::focSelectControlModeId);
    ASSERT_GE(lastSentMsg.size(), 2u);
    EXPECT_EQ(lastSentMsg[1], static_cast<uint8_t>(can::FocMotorMode::speed));
}

TEST_F(FocMotorCategoryClientTest, OnSelectControlModeResponse_NotifiesObserver)
{
    EXPECT_CALL(observer, OnSelectControlModeResponse(3, can::FocMotorMode::position));

    hal::Can::Message msg;
    msg.push_back(static_cast<uint8_t>(can::FocMotorMode::position));
    client.HandleMessage(can::focSelectControlModeResponseId, msg);
}

TEST_F(FocMotorCategoryClientTest, OnCategoryError_NotifiesObserver)
{
    EXPECT_CALL(observer, OnCategoryError(3, can::focSetParameterId, can::FocMotorCategoryError::applicationError));

    hal::Can::Message msg;
    msg.push_back(can::focSetParameterId);
    msg.push_back(static_cast<uint8_t>(can::FocMotorCategoryError::applicationError));
    client.HandleMessage(services::canCategoryErrorResponseMessageTypeId, msg);
}

TEST_F(FocMotorCategoryClientTest, SendStart_PrependSequenceByte)
{
    client.SendStart(1);
    EXPECT_EQ(lastSentMsgType, can::focStartId);
    ASSERT_GE(lastSentMsg.size(), 1u);
}

TEST_F(FocMotorCategoryClientTest, SendCommissioningCommands_TransmitTheirCommandIds)
{
    client.SendAlign(1);
    EXPECT_EQ(lastSentMsgType, can::focAlignId);

    client.SendIdentifyElectrical(1);
    EXPECT_EQ(lastSentMsgType, can::focIdentifyElectricalId);

    client.SendRequestTelemetry(1);
    EXPECT_EQ(lastSentMsgType, can::focRequestTelemetryId);
}

TEST_F(FocMotorCategoryClientTest, SendSelectAlgorithm_EncodesLoopAndAlgorithm)
{
    client.SendSelectAlgorithm(1, can::FocControlLoop::current, static_cast<uint8_t>(can::FocCurrentAlgorithm::slidingMode));

    EXPECT_EQ(lastSentMsgType, can::focSelectAlgorithmId);
    ASSERT_EQ(lastSentMsg.size(), 3u);
    EXPECT_EQ(lastSentMsg[1], 0x00);
    EXPECT_EQ(lastSentMsg[2], 0x03);
}

TEST_F(FocMotorCategoryClientTest, SendSetParameter_EncodesParameterAndFloat)
{
    client.SendSetParameter(1, can::FocParameter::fluxLinkage, 0.007f);

    EXPECT_EQ(lastSentMsgType, can::focSetParameterId);
    EXPECT_EQ(lastSentMsg, MakeMessage({ 0x00, 0x43, 0x3B, 0xE5, 0x60, 0x42 }));
}

TEST_F(FocMotorCategoryClientTest, SendQueries_TransmitTheirCommandIds)
{
    client.SendQueryAlgorithms(1);
    EXPECT_EQ(lastSentMsgType, can::focQueryAlgorithmsId);

    client.SendQueryParameter(1, can::FocParameter::polePairs);
    EXPECT_EQ(lastSentMsgType, can::focQueryParameterId);
    ASSERT_EQ(lastSentMsg.size(), 2u);
    EXPECT_EQ(lastSentMsg[1], 0x42);
}

TEST_F(FocMotorCategoryClientTest, OnAlgorithmsResponse_NotifiesTheSendingNode)
{
    EXPECT_CALL(observer, OnAlgorithmsResponse(3, (can::FocAlgorithms{ can::FocMotorMode::position, can::FocCurrentAlgorithm::deadbeat, can::FocSpeedAlgorithm::twoDof, can::FocPositionAlgorithm::lqi })));

    client.HandleMessage(can::focAlgorithmsResponseId, MakeMessage({ 0x02, 0x02, 0x03, 0x03 }));
}

TEST_F(FocMotorCategoryClientTest, OnParameterResponse_DecodesValueAndAutomaticFlag)
{
    EXPECT_CALL(observer, OnParameterResponse(3, can::FocParameter::currentBandwidth, (can::FocParameterValue{ 1000.0f, true })));

    client.HandleMessage(can::focParameterResponseId, MakeMessage({ 0x00, 0x01, 0x44, 0x7A, 0x00, 0x00 }));
}

TEST_F(FocMotorCategoryClientTest, OnParameterResponse_ShortPayload_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focParameterResponseId, MakeMessage({ 0x00, 0x01, 0x44, 0x7A, 0x00 })));
}

TEST_F(FocMotorCategoryClientTest, OnTelemetryCurrents_DecodesSequenceCurrentsAndPosition)
{
    EXPECT_CALL(observer, OnTelemetryCurrents(3, 0x2A, _, _, _))
        .WillOnce(Invoke([](uint16_t, uint8_t, foc::Ampere id, foc::Ampere iq, foc::Radians position)
            {
                EXPECT_NEAR(id.Value(), 0.512f, 1e-6f);
                EXPECT_NEAR(iq.Value(), -1.0f, 1e-6f);
                EXPECT_NEAR(position.Value(), -3.1416f, 1e-6f);
            }));

    client.HandleMessage(can::focTelemetryCurrentsId, MakeMessage({ 0x2A, 0x02, 0x00, 0xFC, 0x18, 0x85, 0x48 }));
}

TEST_F(FocMotorCategoryClientTest, OnTelemetryCurrents_WrongLength_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focTelemetryCurrentsId, MakeMessage({ 0x2A, 0x02, 0x00, 0xFC, 0x18, 0x85 })));
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focTelemetryCurrentsId, MakeMessage({ 0x2A, 0x02, 0x00, 0xFC, 0x18, 0x85, 0x48, 0x00 })));
}

TEST_F(FocMotorCategoryClientTest, the_retired_electrical_telemetry_frame_is_not_handled)
{
    EXPECT_EQ(services::CanDispatchResult::unknownMessageType, client.HandleMessage(0x88, MakeMessage({ 0x00, 0xF0, 0x00, 0x96, 0x00, 0x0F, 0x00, 0x00 })));
}

TEST_F(FocMotorCategoryClientTest, the_retired_electrical_parameters_response_is_not_handled)
{
    EXPECT_EQ(services::CanDispatchResult::unknownMessageType, client.HandleMessage(0x86, MakeMessage({ 0x01, 0x68, 0x00, 0xC8, 0x07 })));
}

TEST_F(FocMotorCategoryClientTest, SendSetpoint_EncodesTheFloatAfterTheSequenceByte)
{
    EXPECT_TRUE(client.SendSetpoint(1, can::FocSetpoint{ can::FocSetpointKind::speed, 3.25f }));

    EXPECT_EQ(lastSentMsgType, can::focSetSpeedSetpointId);
    EXPECT_EQ(lastSentMsg, MakeMessage({ 0x00, 0x40, 0x50, 0x00, 0x00 }));
}

TEST_F(FocMotorCategoryClientTest, SendSetpoint_EachKindUsesItsOwnCommand)
{
    client.SendSetpoint(1, can::FocSetpoint{ can::FocSetpointKind::torqueCurrent, 1.5f });
    EXPECT_EQ(lastSentMsgType, can::focSetTorqueSetpointId);
    EXPECT_EQ(lastSentMsg, MakeMessage({ 0x00, 0x3F, 0xC0, 0x00, 0x00 }));

    client.SendSetpoint(1, can::FocSetpoint{ can::FocSetpointKind::torqueNewtonMeter, 1.5f });
    EXPECT_EQ(lastSentMsgType, can::focSetTorqueSetpointNewtonMeterId);

    client.SendSetpoint(1, can::FocSetpoint{ can::FocSetpointKind::position, 1.5f });
    EXPECT_EQ(lastSentMsgType, can::focSetPositionSetpointId);
}

TEST_F(FocMotorCategoryClientTest, SendSetpoint_ANonFiniteValueIsNotSent)
{
    lastSentMsgType = 0;

    EXPECT_FALSE(client.SendSetpoint(1, can::FocSetpoint{ can::FocSetpointKind::torqueNewtonMeter, std::numeric_limits<float>::quiet_NaN() }));
    EXPECT_EQ(lastSentMsgType, 0);
}

TEST_F(FocMotorCategoryClientTest, SendSetpoint_ToANodeOfAnotherVersionIsRefused)
{
    EXPECT_CALL(observer, OnContractVersionResponse(3, _, _));
    client.HandleMessage(can::focContractVersionResponseId, MakeMessage({ can::focContractVersionMajor, static_cast<uint8_t>(can::focContractVersionMinor + 1) }));
    lastSentMsgType = 0;

    EXPECT_FALSE(client.SendSetpoint(3, can::FocSetpoint{ can::FocSetpointKind::position, 1.0f }));
    EXPECT_FALSE(client.SendRequestTelemetry(3));
    EXPECT_EQ(lastSentMsgType, 0);
}

TEST_F(FocMotorCategoryClientTest, OnTelemetryMotion_DecodesSpeedAndPositionAtFullResolution)
{
    EXPECT_CALL(observer, OnTelemetryMotion(3, foc::RadiansPerSecond{ 0.005f }, foc::Radians{ 0.00015f }));

    client.HandleMessage(can::focTelemetryMotionId, MakeMessage({ 0x3B, 0xA3, 0xD7, 0x0A, 0x39, 0x1D, 0x49, 0x52 }));
}

TEST_F(FocMotorCategoryClientTest, OnTelemetryMotion_WrongLength_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focTelemetryMotionId, MakeMessage({ 0x40, 0x50, 0x00, 0x00, 0x3F, 0xC0, 0x00 })));
}

TEST_F(FocMotorCategoryClientTest, OnTelemetryMotion_ANonFiniteValue_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focTelemetryMotionId, MakeMessage({ 0x7F, 0xC0, 0x00, 0x00, 0x3F, 0xC0, 0x00, 0x00 })));
}

TEST_F(FocMotorCategoryClientTest, OnTelemetryStatus_DecodesStateAndFaultCode)
{
    EXPECT_CALL(observer, OnTelemetryStatus(3, can::FocMotorState::fault, can::FocFaultCode::overCurrent));

    client.HandleMessage(can::focTelemetryStatusResponseId, MakeMessage({ 0x02, 0x01 }));
}

TEST_F(FocMotorCategoryClientTest, OnTelemetryStatus_AnUnknownState_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focTelemetryStatusResponseId, MakeMessage({ 0x06, 0x00 })));
}

TEST_F(FocMotorCategoryClientTest, OnTelemetryStatus_TheRetiredSixByteLayout_IsRejected)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focTelemetryStatusResponseId, MakeMessage({ 0x01, 0x00, 0x01, 0x2C, 0x01, 0x3A })));
}

TEST_F(FocMotorCategoryClientTest, SendQueryFaultHistory_CarriesTheAge)
{
    client.SendQueryFaultHistory(1, 5);

    EXPECT_EQ(lastSentMsgType, can::focQueryFaultHistoryId);
    ASSERT_EQ(lastSentMsg.size(), 2u);
    EXPECT_EQ(lastSentMsg[1], 5);
}

TEST_F(FocMotorCategoryClientTest, SendClearFaultHistory_TransmitsTheCommand)
{
    client.SendClearFaultHistory(1);

    EXPECT_EQ(lastSentMsgType, can::focClearFaultHistoryId);
    EXPECT_EQ(lastSentMsg.size(), 1u);
}

TEST_F(FocMotorCategoryClientTest, OnFaultRecordResponse_DecodesTheRecordOfThatAge)
{
    EXPECT_CALL(observer, OnFaultRecordResponse(3, 1, can::FocFaultCode::stall, can::FocMotorMode::position, 70000u));

    client.HandleMessage(can::focFaultRecordResponseId, MakeMessage({ 0x01, 0x06, 0x02, 0x00, 0x01, 0x11, 0x70 }));
}

TEST_F(FocMotorCategoryClientTest, OnFaultRecordResponse_NamingNoMode_IsDropped)
{
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focFaultRecordResponseId, MakeMessage({ 0x01, 0x06, 0x03, 0x00, 0x01, 0x11, 0x70 })));
}

TEST_F(FocMotorCategoryClientTest, OnFaultConditionsResponse_DecodesHundredths)
{
    EXPECT_CALL(observer, OnFaultConditionsResponse(3, 0, 7, _, _)).WillOnce(Invoke([](uint16_t, uint8_t, int16_t, foc::Volts busVoltage, foc::Ampere peakCurrent)
        {
            EXPECT_NEAR(24.0f, busVoltage.Value(), 1e-4f);
            EXPECT_NEAR(3.5f, peakCurrent.Value(), 1e-4f);
        }));

    client.HandleMessage(can::focFaultConditionsResponseId, MakeMessage({ 0x00, 0x00, 0x07, 0x09, 0x60, 0x01, 0x5E }));
}
