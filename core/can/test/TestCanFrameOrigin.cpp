#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/CanFrameOrigin.hpp"
#include <gtest/gtest.h>

namespace
{
    using namespace testing;

    class CanFrameOriginTrackerTest
        : public Test
    {
    public:
        CanFrameOriginTrackerTest()
        {
            EXPECT_CALL(canMock, ReceiveData(_)).WillOnce(SaveArg<0>(&busReceive));
            tracker.ReceiveData([this](hal::Can::Id, const hal::Can::Message&)
                {
                    sourceSeenByReceiver = tracker.SourceNodeId();
                });
        }

        static hal::Can::Id FrameFrom(uint16_t node)
        {
            return hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::telemetry, 0x02, 0x89, node));
        }

        StrictMock<hal::CanMock> canMock;
        can::CanFrameOriginTracker tracker{ canMock };
        infra::Function<void(hal::Can::Id, const hal::Can::Message&)> busReceive;
        uint16_t sourceSeenByReceiver{ 0 };
    };
}

TEST_F(CanFrameOriginTrackerTest, reports_the_sender_of_the_frame_being_delivered)
{
    busReceive(FrameFrom(7), hal::Can::Message{});
    EXPECT_EQ(7u, sourceSeenByReceiver);

    busReceive(FrameFrom(4095), hal::Can::Message{});
    EXPECT_EQ(4095u, sourceSeenByReceiver);
    EXPECT_EQ(4095u, tracker.SourceNodeId());
}

TEST_F(CanFrameOriginTrackerTest, standard_identifiers_have_no_sender)
{
    busReceive(hal::Can::Id::Create11BitId(0x123), hal::Can::Message{});
    EXPECT_EQ(0u, sourceSeenByReceiver);
}

TEST_F(CanFrameOriginTrackerTest, forwards_transmissions_unchanged)
{
    bool completed{ false };
    EXPECT_CALL(canMock, SendData(FrameFrom(9), _, _)).WillOnce(Invoke([](hal::Can::Id, const hal::Can::Message&, const infra::Function<void(bool)>& done)
        {
            done(true);
        }));

    tracker.SendData(FrameFrom(9), hal::Can::Message{}, [&completed](bool success)
        {
            completed = success;
        });

    EXPECT_TRUE(completed);
}

TEST_F(CanFrameOriginTrackerTest, deregistration_reaches_the_bus)
{
    EXPECT_CALL(canMock, ReceiveData(_)).WillOnce(Invoke([](const infra::Function<void(hal::Can::Id, const hal::Can::Message&)>& action)
        {
            EXPECT_FALSE(static_cast<bool>(action));
        }));

    tracker.ReceiveData(nullptr);
}
