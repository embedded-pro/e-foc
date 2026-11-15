#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/position_tracking/PositionTracker.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    constexpr float turn = 2.0f * std::numbers::pi_v<float>;

    class TestPositionTracker
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        TestPositionTracker()
        {
            EXPECT_CALL(encoder, Read()).WillRepeatedly(Invoke([this]()
                {
                    return foc::Radians{ angle };
                }));
        }

        void SampleAt(float sampled)
        {
            angle = sampled;
            ForwardTime(services::PositionTracker::samplePeriod);
        }

        StrictMock<drivers::EncoderMock> encoder;
        float angle{ 0.0f };
        services::PositionTracker tracker{ encoder };
    };
}

TEST_F(TestPositionTracker, counts_the_turns_of_an_encoder_sample_every_millisecond)
{
    for (const float sampled : { 0.0f, 2.0f, -2.0f, 0.0f, 2.0f, -2.0f, 0.5f })
        SampleAt(sampled);

    EXPECT_NEAR(2.0f * turn + 0.5f, tracker.Position().Value(), 1e-4f);
}

TEST_F(TestPositionTracker, forwards_a_read_to_the_encoder)
{
    angle = 1.25f;

    EXPECT_FLOAT_EQ(1.25f, tracker.Read().Value());
}

TEST_F(TestPositionTracker, a_new_zero_is_forwarded_and_restarts_the_count_at_turn_zero)
{
    for (const float sampled : { 0.0f, 2.0f, -2.0f, 0.0f })
        SampleAt(sampled);

    EXPECT_CALL(encoder, SetZero());
    tracker.SetZero();
    SampleAt(-3.0f);

    EXPECT_FLOAT_EQ(-3.0f, tracker.Position().Value());
}

TEST_F(TestPositionTracker, a_set_angle_is_forwarded_and_restarts_the_count_at_turn_zero)
{
    for (const float sampled : { 0.0f, 2.0f, -2.0f, 0.0f })
        SampleAt(sampled);

    EXPECT_CALL(encoder, Set(_)).WillOnce(Invoke([](foc::Radians value)
        {
            EXPECT_FLOAT_EQ(0.5f, value.Value());
        }));
    tracker.Set(foc::Radians{ 0.5f });
    SampleAt(0.5f);

    EXPECT_FLOAT_EQ(0.5f, tracker.Position().Value());
}

TEST_F(TestPositionTracker, while_the_cascade_holds_the_count_the_samples_count_nothing)
{
    SampleAt(0.0f);
    tracker.Handover().Hold();

    SampleAt(3.0f);
    SampleAt(-3.0f);

    EXPECT_FLOAT_EQ(0.0f, tracker.Position().Value());
}
