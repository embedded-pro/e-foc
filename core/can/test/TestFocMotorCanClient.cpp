#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCanClient.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gtest/gtest.h>

namespace
{
    using namespace testing;

    class StreamObserver
        : public can::FocMotorCategoryClientObserver
    {
    public:
        using can::FocMotorCategoryClientObserver::FocMotorCategoryClientObserver;

        MOCK_METHOD(void, OnSelectControlModeResponse, (uint16_t sourceNodeId, can::FocMotorMode activeMode), (override));
        MOCK_METHOD(void, OnCategoryError, (uint16_t sourceNodeId, uint8_t originCommandId, can::FocMotorCategoryError errorCode), (override));
        MOCK_METHOD(void, OnTelemetryStatus, (uint16_t sourceNodeId, const hal::Can::Message& msg), (override));
        MOCK_METHOD(void, OnTelemetryCurrents, (uint16_t sourceNodeId, uint8_t sequence, foc::Ampere id, foc::Ampere iq, foc::Radians position), (override));
        MOCK_METHOD(void, OnContractVersionResponse, (uint16_t sourceNodeId, uint8_t major, uint8_t minor), (override));
        MOCK_METHOD(void, OnElectricalParamsResponse, (uint16_t sourceNodeId, foc::Ohm resistance, foc::MilliHenry inductance, uint8_t polePairs), (override));
        MOCK_METHOD(void, OnAlgorithmsResponse, (uint16_t sourceNodeId, const can::FocAlgorithms& algorithms), (override));
        MOCK_METHOD(void, OnParameterResponse, (uint16_t sourceNodeId, can::FocParameter parameter, can::FocParameterValue value), (override));
    };

    hal::Can::Message StreamPayload(uint8_t sequence, int16_t iqMilliAmpere)
    {
        hal::Can::Message msg;
        msg.push_back(sequence);
        msg.resize(7, 0);
        services::CanFrameCodec::WriteInt16(msg, 3, iqMilliAmpere);
        return msg;
    }

    hal::Can::Id StreamId(uint16_t node)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::telemetry, can::focMotorCategoryId, can::focTelemetryCurrentsId, node));
    }

    class FocMotorCanClientTest
        : public Test
        , public infra::ClockFixture
    {
    public:
        StrictMock<hal::CanMock> canMock;

        infra::Execute setupExpectations{ [this]()
            {
                EXPECT_CALL(canMock, SendData(_, _, _))
                    .Times(AnyNumber())
                    .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message&, const infra::Function<void(bool)>& cb)
                        {
                            lastSentMsgType = services::ExtractCanMessageType(id.Get29BitId());
                            lastSentNodeId = services::ExtractCanNodeId(id.Get29BitId());
                            cb(true);
                        }));
                EXPECT_CALL(canMock, ReceiveData(_)).Times(AnyNumber()).WillRepeatedly(SaveArg<0>(&receive));
            } };

        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> receive;
        can::FocMotorCanClient client{ canMock };

        uint8_t lastSentMsgType{};
        uint16_t lastSentNodeId{};
    };
}

TEST_F(FocMotorCanClientTest, CategoryClientIsRegisteredWithProtocolClient)
{
    EXPECT_EQ(client.CategoryClient().Id(), can::focMotorCategoryId);
}

TEST_F(FocMotorCanClientTest, CategoryClientSendsThroughTheBus)
{
    client.CategoryClient().SendStart(5);

    EXPECT_EQ(lastSentMsgType, can::focStartId);
    EXPECT_EQ(lastSentNodeId, 5u);
}

TEST_F(FocMotorCanClientTest, stream_frames_interleaved_from_several_nodes_are_attributed_to_their_sender)
{
    StrictMock<StreamObserver> observer{ client.CategoryClient() };

    InSequence order;
    EXPECT_CALL(observer, OnTelemetryCurrents(3, 7, _, Property(&foc::Ampere::Value, FloatEq(0.3f)), _));
    EXPECT_CALL(observer, OnTelemetryCurrents(9, 40, _, Property(&foc::Ampere::Value, FloatEq(-0.9f)), _));
    EXPECT_CALL(observer, OnTelemetryCurrents(3, 8, _, Property(&foc::Ampere::Value, FloatEq(0.31f)), _));
    EXPECT_CALL(observer, OnTelemetryCurrents(5, 200, _, Property(&foc::Ampere::Value, FloatEq(0.5f)), _));
    EXPECT_CALL(observer, OnTelemetryCurrents(9, 41, _, Property(&foc::Ampere::Value, FloatEq(-0.91f)), _));

    ASSERT_TRUE(receive);
    receive(StreamId(3), StreamPayload(7, 300));
    receive(StreamId(9), StreamPayload(40, -900));
    receive(StreamId(3), StreamPayload(8, 310));
    receive(StreamId(5), StreamPayload(200, 500));
    receive(StreamId(9), StreamPayload(41, -910));
}
