#include "core/supervision/MotorOverload.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

namespace
{
    using namespace testing;

    class FocStateMachineBaseMock
        : public state_machine::FocStateMachineBase
    {
    public:
        MOCK_METHOD(const state_machine::State&, CurrentState, (), (const, override));
        MOCK_METHOD(state_machine::FaultCode, LastFaultCode, (), (const, override));
        MOCK_METHOD(bool, HasPendingAsyncWork, (), (const, override));
        MOCK_METHOD(bool, HasPartialCalibration, (), (const, override));
        MOCK_METHOD(void, CmdCalibrate, (const infra::Function<void(state_machine::CommandResult)>& onDone), (override));
        MOCK_METHOD(state_machine::CommandResult, CmdEnable, (), (override));
        MOCK_METHOD(state_machine::CommandResult, CmdDisable, (), (override));
        MOCK_METHOD(state_machine::CommandResult, CmdClearFault, (), (override));
        MOCK_METHOD(void, CmdClearCalibration, (const infra::Function<void(state_machine::CommandResult)>& onDone), (override));
        MOCK_METHOD(state_machine::CommandResult, CmdEmergencyStop, (), (override));
        MOCK_METHOD(foc::MotionObservation, ObserveMotion, (), (const, override));
    };

    class MotorOverloadTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static constexpr float boardLimit{ 10.0f };

        MotorOverloadTest()
        {
            EXPECT_CALL(drive, ObserveMotion()).WillRepeatedly(Invoke([this]()
                {
                    foc::MotionObservation observation{};
                    observation.measuredTorqueCurrent = foc::Ampere{ current };
                    return observation;
                }));

            overload.Attach([this]() -> const state_machine::FocStateMachineBase&
                {
                    return drive;
                });
            overload.Enable([this]()
                {
                    return ratings;
                });
        }

        void Run(std::chrono::milliseconds duration)
        {
            ForwardTime(duration);
        }

        std::optional<std::chrono::milliseconds> MillisecondsUntilLimit(float limit, std::chrono::milliseconds longest)
        {
            for (std::chrono::milliseconds elapsed{ 0 }; elapsed < longest; elapsed += std::chrono::milliseconds(1))
            {
                if (overload.Limit().Value() == limit)
                    return elapsed;
                Run(std::chrono::milliseconds(1));
            }
            return std::nullopt;
        }

        StrictMock<FocStateMachineBaseMock> drive;
        float current{ 0.0f };
        supervision::MotorOverload::Ratings ratings{ foc::Ampere{ 2.0f }, foc::Ampere{ 3.0f }, std::chrono::seconds(2) };
        std::vector<float> limits;
        supervision::MotorOverload overload{ foc::Ampere{ boardLimit }, [this](foc::Ampere limit)
            {
                limits.push_back(limit.Value());
            } };
    };
}

TEST_F(MotorOverloadTest, without_a_continuous_rating_the_board_limit_stays_in_force)
{
    ratings.continuous = foc::Ampere{ 0.0f };
    current = 10.0f;

    Run(std::chrono::seconds(10));

    EXPECT_TRUE(limits.empty());
    EXPECT_NEAR(overload.Limit().Value(), boardLimit, 1e-6f);
}

TEST_F(MotorOverloadTest, a_cold_motor_may_carry_its_peak_current)
{
    Run(std::chrono::milliseconds(1));

    EXPECT_EQ(std::vector<float>{ 3.0f }, limits);
}

TEST_F(MotorOverloadTest, the_peak_current_is_allowed_for_the_peak_time_and_then_folds_back_to_the_continuous_rating)
{
    current = 3.0f;
    Run(std::chrono::milliseconds(1));

    const auto folded = MillisecondsUntilLimit(2.0f, std::chrono::seconds(5));

    ASSERT_TRUE(folded.has_value());
    EXPECT_NEAR(static_cast<float>(folded->count()), 2000.0f, 200.0f);
}

TEST_F(MotorOverloadTest, a_motor_at_its_continuous_rating_stays_folded_back)
{
    current = 3.0f;
    Run(std::chrono::seconds(3));
    current = 2.0f;

    Run(std::chrono::seconds(30));

    EXPECT_NEAR(overload.Limit().Value(), 2.0f, 1e-6f);
}

TEST_F(MotorOverloadTest, the_peak_returns_once_the_motor_has_cooled_to_half_its_continuous_load)
{
    current = 3.0f;
    Run(std::chrono::seconds(3));
    current = 2.0f;
    Run(std::chrono::seconds(30));
    current = 0.0f;

    const auto released = MillisecondsUntilLimit(3.0f, std::chrono::seconds(5));

    ASSERT_TRUE(released.has_value());
    EXPECT_NEAR(static_cast<float>(released->count()), 2357.0f, 100.0f);
}

TEST_F(MotorOverloadTest, a_continuous_rating_at_or_above_the_peak_is_a_flat_limit)
{
    ratings = { foc::Ampere{ 4.0f }, foc::Ampere{ 3.0f }, std::chrono::seconds(2) };
    current = 3.0f;

    Run(std::chrono::seconds(10));

    EXPECT_EQ(std::vector<float>{ 3.0f }, limits);
}

TEST_F(MotorOverloadTest, a_peak_above_the_board_limit_is_held_at_the_board_limit)
{
    ratings = { foc::Ampere{ 2.0f }, foc::Ampere{ 30.0f }, std::chrono::seconds(2) };

    Run(std::chrono::milliseconds(1));

    EXPECT_TRUE(limits.empty());
    EXPECT_NEAR(overload.Limit().Value(), boardLimit, 1e-6f);
}

TEST_F(MotorOverloadTest, a_peak_of_zero_is_the_board_limit)
{
    ratings = { foc::Ampere{ 2.0f }, foc::Ampere{ 0.0f }, std::chrono::seconds(2) };

    Run(std::chrono::milliseconds(1));

    EXPECT_TRUE(limits.empty());
    EXPECT_NEAR(overload.Limit().Value(), boardLimit, 1e-6f);
}

TEST_F(MotorOverloadTest, a_reverse_current_heats_the_motor_as_much)
{
    current = -3.0f;
    Run(std::chrono::milliseconds(1));

    const auto folded = MillisecondsUntilLimit(2.0f, std::chrono::seconds(5));

    ASSERT_TRUE(folded.has_value());
    EXPECT_NEAR(static_cast<float>(folded->count()), 2000.0f, 200.0f);
}
