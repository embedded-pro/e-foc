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
                            lastSentNodeId = services::ExtractCanNodeId(id.Get29BitId());
                            cb(true);
                        }));
                EXPECT_CALL(canMock, ReceiveData(_)).Times(AnyNumber());
            } };

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
