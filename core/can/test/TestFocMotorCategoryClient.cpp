#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceSource.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCategoryClient.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "infra/util/Function.hpp"
#include <gtest/gtest.h>
#include <optional>

namespace
{
    using namespace testing;

    struct SequenceSourceStub
        : services::CanSequenceSource
    {
        uint8_t PeekSequence(uint16_t) override
        {
            return seq;
        }

        void CommitSequence(uint16_t, uint8_t, uint8_t) override
        {
            ++seq;
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
        MOCK_METHOD(void, OnTelemetryStatus, (uint16_t sourceNodeId, const hal::Can::Message& msg), (override));
        MOCK_METHOD(void, OnTelemetryElectrical, (uint16_t sourceNodeId, const hal::Can::Message& msg), (override));
        MOCK_METHOD(void, OnContractVersionResponse, (uint16_t sourceNodeId, uint8_t major, uint8_t minor), (override));
        MOCK_METHOD(void, OnElectricalParamsResponse, (uint16_t sourceNodeId, foc::Ohm resistance, foc::MilliHenry inductance, uint8_t polePairs), (override));
        MOCK_METHOD(void, OnAlgorithmsResponse, (uint16_t sourceNodeId, const can::FocAlgorithms& algorithms), (override));
        MOCK_METHOD(void, OnParameterResponse, (uint16_t sourceNodeId, can::FocParameter parameter, can::FocParameterValue value), (override));
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

TEST_F(FocMotorCategoryClientTest, SendSetTorqueSetpoint_EncodesCurrent)
{
    client.SendSetTorqueSetpoint(1, foc::Ampere{ 1.5f });
    EXPECT_EQ(lastSentMsgType, can::focSetTorqueSetpointId);
    ASSERT_GE(lastSentMsg.size(), 3u);
    const auto wireVal = services::CanFrameCodec::ReadInt16(lastSentMsg, 1);
    EXPECT_NEAR(static_cast<float>(wireVal) / can::focCurrentScale, 1.5f, 0.01f);
}

TEST_F(FocMotorCategoryClientTest, SendSetSpeedSetpoint_EncodesSpeed)
{
    client.SendSetSpeedSetpoint(1, foc::RadiansPerSecond{ 300.0f });
    EXPECT_EQ(lastSentMsgType, can::focSetSpeedSetpointId);
    ASSERT_GE(lastSentMsg.size(), 3u);
    const auto wireVal = services::CanFrameCodec::ReadInt16(lastSentMsg, 1);
    EXPECT_NEAR(static_cast<float>(wireVal) / can::focSpeedScale, 300.0f, 0.1f);
}

TEST_F(FocMotorCategoryClientTest, SendSetPositionSetpoint_EncodesPosition)
{
    client.SendSetPositionSetpoint(1, foc::Radians{ 3.14f });
    EXPECT_EQ(lastSentMsgType, can::focSetPositionSetpointId);
    ASSERT_GE(lastSentMsg.size(), 3u);
    const auto wireVal = services::CanFrameCodec::ReadInt16(lastSentMsg, 1);
    EXPECT_NEAR(static_cast<float>(wireVal) / can::focPositionScale, 3.14f, 0.01f);
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
    EXPECT_CALL(observer, OnCategoryError(3, can::focSetPidCurrentId, can::FocMotorCategoryError::applicationError));

    hal::Can::Message msg;
    msg.push_back(can::focSetPidCurrentId);
    msg.push_back(static_cast<uint8_t>(can::FocMotorCategoryError::applicationError));
    client.HandleMessage(services::canCategoryErrorResponseMessageTypeId, msg);
}

TEST_F(FocMotorCategoryClientTest, SendSetCurrentBandwidth_EncodesCorrectCommandAndValue)
{
    client.SendSetCurrentBandwidth(1, 500.0f);
    EXPECT_EQ(lastSentMsgType, can::focSetPidCurrentId);
    ASSERT_GE(lastSentMsg.size(), 3u);
    const auto wireVal = services::CanFrameCodec::ReadInt16(lastSentMsg, 1);
    EXPECT_NEAR(static_cast<float>(wireVal) / can::focPidScale, 500.0f, 1.0f);
}

TEST_F(FocMotorCategoryClientTest, SendSetSpeedBandwidth_EncodesCorrectCommandAndValue)
{
    client.SendSetSpeedBandwidth(1, 188.5f);
    EXPECT_EQ(lastSentMsgType, can::focSetPidSpeedId);
    ASSERT_GE(lastSentMsg.size(), 3u);
    const auto wireVal = services::CanFrameCodec::ReadInt16(lastSentMsg, 1);
    EXPECT_NEAR(static_cast<float>(wireVal) / can::focPidScale, 188.5f, 1.0f);
}

TEST_F(FocMotorCategoryClientTest, SendSetPositionBandwidth_EncodesCorrectCommandAndValue)
{
    client.SendSetPositionBandwidth(1, 18.8f);
    EXPECT_EQ(lastSentMsgType, can::focSetPidPositionId);
    ASSERT_GE(lastSentMsg.size(), 3u);
    const auto wireVal = services::CanFrameCodec::ReadInt16(lastSentMsg, 1);
    EXPECT_NEAR(static_cast<float>(wireVal) / can::focPidScale, 18.8f, 1.0f);
}

TEST_F(FocMotorCategoryClientTest, SendSetCurrentBandwidth_PayloadIsExactlyOneFixed16Field)
{
    client.SendSetCurrentBandwidth(1, 300.0f);
    EXPECT_EQ(lastSentMsg.size(), 3u);
}

TEST_F(FocMotorCategoryClientTest, SendStart_PrependSequenceByte)
{
    client.SendStart(1);
    EXPECT_EQ(lastSentMsgType, can::focStartId);
    ASSERT_GE(lastSentMsg.size(), 1u);
}

TEST_F(FocMotorCategoryClientTest, OnElectricalParamsResponse_DecodesResistanceInductanceAndPolePairs)
{
    foc::Ohm resistance{ 0.0f };
    foc::MilliHenry inductance{ 0.0f };
    uint8_t polePairs{};
    EXPECT_CALL(observer, OnElectricalParamsResponse(3, _, _, _))
        .WillOnce(Invoke([&](uint16_t, foc::Ohm r, foc::MilliHenry l, uint8_t p)
            {
                resistance = r;
                inductance = l;
                polePairs = p;
            }));

    const auto msg = MakeMessage({ 0x01, 0xF4, 0x03, 0xE8, 0x07 });
    client.HandleMessage(can::focElectricalParamsResponseId, msg);

    EXPECT_NEAR(resistance.Value(), 500.0f / can::focResistanceScale, 0.001f);
    EXPECT_NEAR(inductance.Value(), 1000.0f / can::focInductanceScale, 0.001f);
    EXPECT_EQ(polePairs, 7);
}

TEST_F(FocMotorCategoryClientTest, OnElectricalParamsResponse_ShortPayload_IsRejected)
{
    const auto msg = MakeMessage({ 0x01, 0xF4, 0x03, 0xE8 });
    EXPECT_EQ(services::CanDispatchResult::rejected, client.HandleMessage(can::focElectricalParamsResponseId, msg));
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

TEST_F(FocMotorCategoryClientTest, SendSetEncoderResolution_EncodesCountsPerRevolution)
{
    client.SendSetEncoderResolution(1, 4096);

    EXPECT_EQ(lastSentMsgType, can::focSetEncoderResolutionId);
    ASSERT_EQ(lastSentMsg.size(), 5u);
    EXPECT_EQ(services::CanFrameCodec::ReadUInt32(lastSentMsg, 1), 4096u);
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
