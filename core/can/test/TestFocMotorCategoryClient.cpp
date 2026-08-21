#pragma GCC optimize("O3", "fast-math")

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

    struct SequenceSourceStub : services::CanSequenceSource
    {
        uint8_t PeekSequence(uint16_t) override { return seq; }
        void CommitSequence(uint16_t, uint8_t, uint8_t) override { ++seq; }
        uint8_t seq{};
    };

    class MockClientObserver : public can::FocMotorCategoryClientObserver
    {
    public:
        using can::FocMotorCategoryClientObserver::FocMotorCategoryClientObserver;

        MOCK_METHOD(void, OnSelectControlModeResponse, (can::FocMotorMode activeMode), (override));
        MOCK_METHOD(void, OnCategoryError, (uint8_t originCommandId, can::FocMotorCategoryError errorCode), (override));
        MOCK_METHOD(void, OnTelemetryStatus, (const hal::Can::Message& msg), (override));
        MOCK_METHOD(void, OnTelemetryElectrical, (const hal::Can::Message& msg), (override));
    };

    class FocMotorCategoryClientTest : public Test
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

        StrictMock<hal::CanMock> canMock;
        services::CanFrameTransport transport{ canMock, 0 };
        SequenceSourceStub seqSource;
        can::FocMotorCategoryClient client{ transport, seqSource };
        StrictMock<MockClientObserver> observer{ client };

        uint8_t lastSentMsgType{};
        hal::Can::Message lastSentMsg;
    };

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
        EXPECT_CALL(observer, OnSelectControlModeResponse(can::FocMotorMode::position));

        hal::Can::Message msg;
        msg.push_back(static_cast<uint8_t>(can::FocMotorMode::position));
        client.HandleMessage(can::focSelectControlModeResponseId, msg);
    }

    TEST_F(FocMotorCategoryClientTest, OnCategoryError_NotifiesObserver)
    {
        EXPECT_CALL(observer, OnCategoryError(can::focSetPidCurrentId, can::FocMotorCategoryError::applicationError));

        hal::Can::Message msg;
        msg.push_back(can::focSetPidCurrentId);
        msg.push_back(static_cast<uint8_t>(can::FocMotorCategoryError::applicationError));
        client.HandleMessage(services::canCategoryErrorResponseMessageTypeId, msg);
    }

    // REQ-INT-010 — client routes ACK/NACK; sequence byte is prepended
    TEST_F(FocMotorCategoryClientTest, SendStart_PrependSequenceByte)
    {
        client.SendStart(1);
        EXPECT_EQ(lastSentMsgType, can::focStartId);
        ASSERT_GE(lastSentMsg.size(), 1u);
    }
}
