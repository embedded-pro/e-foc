#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/standstill/RotorStandstillImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gmock/gmock.h>
#include <numbers>
#include <optional>

namespace
{
    using namespace testing;

    class TestRotorStandstill
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        TestRotorStandstill()
        {
            EXPECT_CALL(encoder, Read()).WillRepeatedly(Invoke([this]()
                {
                    return foc::Radians{ position };
                }));
        }

        void Await()
        {
            standstill.Await([this](bool atRest)
                {
                    result = atRest;
                });
        }

        void TurnFor(float step, int polls)
        {
            for (int poll = 0; poll < polls; ++poll)
            {
                position += step;
                ForwardTime(services::RotorStandstillImpl::pollPeriod);
            }
        }

        StrictMock<drivers::EncoderMock> encoder;
        services::RotorStandstillImpl standstill{ encoder };
        float position{ 1.0f };
        std::optional<bool> result;
    };
}

TEST_F(TestRotorStandstill, a_rotor_that_holds_still_is_at_rest_after_ten_polls)
{
    Await();

    ForwardTime(services::RotorStandstillImpl::pollPeriod * 9);
    EXPECT_FALSE(result.has_value());

    ForwardTime(services::RotorStandstillImpl::pollPeriod);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(TestRotorStandstill, a_turning_rotor_is_at_rest_only_ten_polls_after_it_stops)
{
    Await();

    TurnFor(0.01f, 50);
    EXPECT_FALSE(result.has_value());

    ForwardTime(services::RotorStandstillImpl::pollPeriod * 10);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(TestRotorStandstill, a_creep_below_the_band_per_poll_is_not_rest)
{
    Await();

    TurnFor(0.0005f, 200);

    EXPECT_FALSE(result.has_value());
}

TEST_F(TestRotorStandstill, jitter_inside_the_band_is_rest)
{
    Await();

    for (int poll = 0; poll < 10; ++poll)
    {
        position = 1.0f + (poll % 2 == 0 ? 0.0015f : -0.0015f);
        ForwardTime(services::RotorStandstillImpl::pollPeriod);
    }

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(TestRotorStandstill, a_rotor_resting_across_the_wrap_is_at_rest)
{
    position = 2.0f * std::numbers::pi_v<float> - 0.0005f;
    Await();

    position = 0.0005f;
    ForwardTime(services::RotorStandstillImpl::pollPeriod * 10);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(TestRotorStandstill, a_rotor_that_never_rests_is_reported_at_the_timeout)
{
    Await();

    TurnFor(0.01f, 999);
    EXPECT_FALSE(result.has_value());

    TurnFor(0.01f, 1);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
}

TEST_F(TestRotorStandstill, an_aborted_wait_reports_nothing)
{
    Await();

    standstill.Abort();
    ForwardTime(services::RotorStandstillImpl::timeout);

    EXPECT_FALSE(result.has_value());
}

TEST_F(TestRotorStandstill, a_second_request_while_waiting_is_refused_and_the_first_completes)
{
    Await();

    std::optional<bool> second;
    standstill.Await([&second](bool atRest)
        {
            second = atRest;
        });

    ASSERT_TRUE(second.has_value());
    EXPECT_FALSE(*second);

    ForwardTime(services::RotorStandstillImpl::pollPeriod * 10);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(TestRotorStandstill, a_completed_wait_can_be_started_again)
{
    Await();
    ForwardTime(services::RotorStandstillImpl::pollPeriod * 10);
    result.reset();

    Await();
    ForwardTime(services::RotorStandstillImpl::pollPeriod * 10);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}
