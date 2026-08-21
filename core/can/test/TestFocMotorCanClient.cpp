#pragma GCC optimize("O3", "fast-math")

#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCanClient.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gtest/gtest.h>

namespace
{
    using namespace testing;

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
                            cb(true);
                        }));
                EXPECT_CALL(canMock, ReceiveData(_)).Times(AnyNumber());
            } };

        can::FocMotorCanClient client{ canMock, 1 };

        uint8_t lastSentMsgType{};
    };

    // REQ-INT-008 — client sends Start and the frame is queued
    TEST_F(FocMotorCanClientTest, Start_TransmitsStartFrame)
    {
        client.Start();
        EXPECT_EQ(lastSentMsgType, can::focStartId);
    }

    TEST_F(FocMotorCanClientTest, Stop_TransmitsStopFrame)
    {
        client.Stop();
        EXPECT_EQ(lastSentMsgType, can::focStopId);
    }

    TEST_F(FocMotorCanClientTest, ClearFault_TransmitsClearFaultFrame)
    {
        client.ClearFault();
        EXPECT_EQ(lastSentMsgType, can::focClearFaultId);
    }

    TEST_F(FocMotorCanClientTest, EmergencyStop_TransmitsEmergencyStopFrame)
    {
        client.EmergencyStop();
        EXPECT_EQ(lastSentMsgType, can::focEmergencyStopId);
    }

    // REQ-INT-009 — client sends typed setpoints
    TEST_F(FocMotorCanClientTest, SetTorque_TransmitsTorqueSetpointFrame)
    {
        client.SetTorque(foc::Ampere{ 1.5f });
        EXPECT_EQ(lastSentMsgType, can::focSetTorqueSetpointId);
    }

    TEST_F(FocMotorCanClientTest, SetSpeed_TransmitsSpeedSetpointFrame)
    {
        client.SetSpeed(foc::RadiansPerSecond{ 100.0f });
        EXPECT_EQ(lastSentMsgType, can::focSetSpeedSetpointId);
    }

    TEST_F(FocMotorCanClientTest, SetPosition_TransmitsPositionSetpointFrame)
    {
        client.SetPosition(foc::Radians{ 1.0f });
        EXPECT_EQ(lastSentMsgType, can::focSetPositionSetpointId);
    }

    TEST_F(FocMotorCanClientTest, SelectControlMode_TransmitsSelectControlModeFrame)
    {
        client.SelectControlMode(can::FocMotorMode::speed);
        EXPECT_EQ(lastSentMsgType, can::focSelectControlModeId);
    }

    TEST_F(FocMotorCanClientTest, CategoryClientIsRegisteredWithProtocolClient)
    {
        EXPECT_EQ(client.CategoryClient().Id(), can::focMotorCategoryId);
    }
}
