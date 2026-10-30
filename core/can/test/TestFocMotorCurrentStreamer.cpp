#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorCurrentStreamer.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/foc/cascade/CurrentStream.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace
{
    using namespace testing;

    constexpr uint32_t controlFrequencyHz{ 20000 };

    struct StreamFrame
    {
        uint8_t sequence;
        float id;
        float iq;
        float position;
    };

    class FocMotorCurrentStreamerTest
        : public Test
        , public infra::ClockFixture
    {
    public:
        static StreamFrame Decode(const hal::Can::Message& msg)
        {
            EXPECT_EQ(msg.size(), 7u);
            return StreamFrame{ msg[0],
                services::CanFrameCodec::Fixed16ToFloat(services::CanFrameCodec::ReadInt16(msg, 1), can::focStreamCurrentScale),
                services::CanFrameCodec::Fixed16ToFloat(services::CanFrameCodec::ReadInt16(msg, 3), can::focStreamCurrentScale),
                services::CanFrameCodec::Fixed16ToFloat(services::CanFrameCodec::ReadInt16(msg, 5), can::focStreamPositionScale) };
        }

        void ControlSamples(uint32_t count, float id = 0.0f, float iq = 0.0f, float position = 0.0f)
        {
            for (uint32_t i = 0; i != count; ++i)
                stream.Accumulate(id, iq, position);
        }

        void CompleteHeldFrame()
        {
            ASSERT_FALSE(heldCompletions.empty());
            auto onDone = heldCompletions.front();
            heldCompletions.erase(heldCompletions.begin());
            onDone(true);
        }

        void OnSend(hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& onDone)
        {
            if (services::ExtractCanMessageType(id.Get29BitId()) != can::focTelemetryCurrentsId || services::ExtractCanCategory(id.Get29BitId()) != can::focMotorCategoryId)
            {
                onDone(true);
                return;
            }

            frames.push_back(Decode(msg));
            if (holdStreamFrames)
                heldCompletions.push_back(onDone);
            else
                onDone(true);
        }

        StrictMock<hal::CanMock> canMock;
        infra::Execute setupExpectations{ [this]()
            {
                EXPECT_CALL(canMock, ReceiveData(_)).Times(AnyNumber());
                EXPECT_CALL(canMock, SendData(_, _, _)).Times(AnyNumber()).WillRepeatedly(Invoke(this, &FocMotorCurrentStreamerTest::OnSend));
            } };
        services::CanProtocolServer protocolServer{ canMock, services::CanProtocolServer::Config{ .nodeId = 1 } };
        can::FocMotorCategoryServer server{ protocolServer.Transport() };
        foc::CurrentStream stream;
        services::ConfigData config;
        can::FocMotorCurrentStreamer streamer{ protocolServer, server, stream, config, hal::Hertz{ controlFrequencyHz } };

        std::vector<StreamFrame> frames;
        bool holdStreamFrames{ false };
        std::vector<infra::Function<void(bool)>> heldCompletions;
    };
}

TEST_F(FocMotorCurrentStreamerTest, nothing_is_streamed_while_no_client_is_online)
{
    config.telemetryRateHz = 1000;

    ControlSamples(100);
    ForwardTime(std::chrono::milliseconds(10));

    EXPECT_TRUE(frames.empty());
}

TEST_F(FocMotorCurrentStreamerTest, an_online_client_receives_the_window_mean_at_the_configured_rate)
{
    config.telemetryRateHz = 1000;
    streamer.Online();

    ControlSamples(10, 0.1f, 1.0f, 0.5f);
    ControlSamples(10, 0.3f, 2.0f, -1.25f);
    ForwardTime(std::chrono::milliseconds(1));

    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].sequence, 0);
    EXPECT_NEAR(frames[0].id, 0.2f, 0.001f);
    EXPECT_NEAR(frames[0].iq, 1.5f, 0.001f);
    EXPECT_NEAR(frames[0].position, -1.25f, 0.0001f);
}

TEST_F(FocMotorCurrentStreamerTest, the_window_is_the_control_periods_of_one_stream_period)
{
    config.telemetryRateHz = 100;
    streamer.Online();

    ControlSamples(199);
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(frames.empty());

    ControlSamples(1);
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_EQ(frames.size(), 1u);
}

TEST_F(FocMotorCurrentStreamerTest, a_rate_that_does_not_divide_the_control_rate_takes_the_nearest_whole_window)
{
    config.telemetryRateHz = 300;
    streamer.Online();

    ControlSamples(66);
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(frames.empty());

    ControlSamples(1);
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_EQ(frames.size(), 1u);
}

TEST_F(FocMotorCurrentStreamerTest, a_zero_rate_streams_nothing)
{
    config.telemetryRateHz = 0;
    streamer.Online();

    ControlSamples(1000);
    ForwardTime(std::chrono::milliseconds(10));

    EXPECT_TRUE(frames.empty());
}

TEST_F(FocMotorCurrentStreamerTest, a_stored_rate_above_the_maximum_streams_at_the_maximum)
{
    config.telemetryRateHz = 5000;
    streamer.Online();

    ControlSamples(20);
    ForwardTime(std::chrono::milliseconds(1));

    EXPECT_EQ(frames.size(), 1u);
}

TEST_F(FocMotorCurrentStreamerTest, a_new_rate_takes_effect_at_the_next_tick)
{
    config.telemetryRateHz = 1000;
    streamer.Online();

    config.telemetryRateHz = 500;
    ForwardTime(std::chrono::milliseconds(1));
    ControlSamples(40);
    ForwardTime(std::chrono::milliseconds(1));

    EXPECT_EQ(frames.size(), 1u);
}

TEST_F(FocMotorCurrentStreamerTest, every_window_is_sent_in_order)
{
    config.telemetryRateHz = 1000;
    streamer.Online();

    for (int window = 0; window != 5; ++window)
    {
        ControlSamples(20);
        ForwardTime(std::chrono::milliseconds(1));
    }

    ASSERT_EQ(frames.size(), 5u);
    for (uint8_t i = 0; i != 5; ++i)
        EXPECT_EQ(frames[i].sequence, i);
}

TEST_F(FocMotorCurrentStreamerTest, a_client_going_offline_stops_the_stream)
{
    config.telemetryRateHz = 1000;
    streamer.Online();
    streamer.Offline();

    ControlSamples(100);
    ForwardTime(std::chrono::milliseconds(10));

    EXPECT_TRUE(frames.empty());
}

TEST_F(FocMotorCurrentStreamerTest, at_most_two_stream_frames_wait_in_the_send_queue)
{
    config.telemetryRateHz = 1000;
    holdStreamFrames = true;
    streamer.Online();

    ControlSamples(4 * 20);
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_EQ(frames.size(), 1u);

    CompleteHeldFrame();
    EXPECT_EQ(frames.size(), 2u);
    CompleteHeldFrame();
    EXPECT_EQ(frames.size(), 2u);

    ForwardTime(std::chrono::milliseconds(1));
    CompleteHeldFrame();
    CompleteHeldFrame();

    ASSERT_EQ(frames.size(), 4u);
    for (uint8_t i = 0; i != 4; ++i)
        EXPECT_EQ(frames[i].sequence, i);
}

TEST_F(FocMotorCurrentStreamerTest, a_heartbeat_from_an_online_client_does_not_restart_the_stream)
{
    config.telemetryRateHz = 1000;
    streamer.Online();

    ControlSamples(10, 0.0f, 1.0f);
    streamer.Online();
    ControlSamples(10, 0.0f, 3.0f);
    ForwardTime(std::chrono::milliseconds(1));

    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].sequence, 0);
    EXPECT_NEAR(frames[0].iq, 2.0f, 0.001f);
}

TEST_F(FocMotorCurrentStreamerTest, a_client_back_online_after_going_offline_restarts_the_stream)
{
    config.telemetryRateHz = 1000;
    streamer.Online();
    streamer.Offline();
    streamer.Online();

    ControlSamples(20, 0.0f, 1.0f);
    ForwardTime(std::chrono::milliseconds(1));

    EXPECT_EQ(frames.size(), 1u);
}
