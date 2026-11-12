#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{
    constexpr float profileVelocity{ 20.0f };
    constexpr float profileAcceleration{ 400.0f };
    constexpr float move{ 1.5f };
    constexpr float periodSeconds{ 0.001f };
    constexpr float followingErrorLimit{ 0.02f };

    struct Observation
    {
        float positionError;
        float targetError;
    };

    struct LawRow
    {
        foc::PositionAlgorithm algorithm;
        const char* name;
    };

    constexpr std::array<LawRow, 5> laws{ {
        { foc::PositionAlgorithm::pid, "pid" },
        { foc::PositionAlgorithm::cascadeP, "cascadep" },
        { foc::PositionAlgorithm::lqr, "lqr" },
        { foc::PositionAlgorithm::lqi, "lqi" },
        { foc::PositionAlgorithm::twoDof, "twodof" },
    } };

    constexpr float profileJerk{ 20000.0f };
    constexpr float scurveFollowingErrorLimit{ 0.04f };

    mil::Scenario Profiled(const std::string& plant, foc::PositionAlgorithm algorithm, float velocity = profileVelocity, float acceleration = profileAcceleration, float jerk = 0.0f)
    {
        auto scenario = mil::Scenario{}.Plant(plant).PositionLoop(algorithm).Recorded(1000, 2000).Calibrated().BootsIn(state_machine::ControlMode::position);
        scenario.nvm.config.limits.profileVelocity = velocity;
        scenario.nvm.config.limits.profileAcceleration = acceleration;
        scenario.nvm.config.limits.profileJerk = jerk;
        return scenario;
    }

    class Session
    {
    public:
        explicit Session(const mil::Scenario& scenario)
            : mil{ scenario }
        {
            mil.ObserveEachMillisecond([this]()
                {
                    const auto observation = mil.ControlMode().Drive().ActiveStateMachine().ObserveMotion();
                    observed.push_back({ observation.positionError.Value(), observation.targetError.Value() });
                });
        }

        bool Settle(float position)
        {
            return mil.Align() && mil.SetPosition(position) && mil.Enable() && mil.RunUntil([this]()
                                                                                   {
                                                                                       return observed.size() > 300;
                                                                                   },
                                                                                   std::chrono::seconds{ 1 });
        }

        std::size_t Mark()
        {
            return observed.size();
        }

        void Run(std::size_t milliseconds)
        {
            mil.Run(std::chrono::milliseconds{ milliseconds });
        }

        mil::ModelInTheLoop mil;
        std::vector<Observation> observed;
    };

    std::vector<float> RemainingReference(const std::vector<Observation>& observed, std::size_t from)
    {
        std::vector<float> remaining;
        for (std::size_t i = from; i != observed.size(); ++i)
            remaining.push_back(observed[i].targetError - observed[i].positionError);
        return remaining;
    }

    std::size_t Duration(const std::vector<float>& remaining)
    {
        const auto started = std::ranges::find_if(remaining, [](float value)
            {
                return std::abs(value - move) > 1e-6f;
            });
        const auto arrived = std::ranges::find_if(remaining, [](float value)
            {
                return std::abs(value) < 1e-4f;
            });
        if (started == remaining.end() || arrived == remaining.end())
            return 0;

        return static_cast<std::size_t>(arrived - started) + 1;
    }
}

TEST(TestPositionProfile, every_position_law_moves_1_5_rad_in_125_ms_and_follows_the_cruise_within_20_mrad)
{
    for (const std::string plant : { "nominal", "anaheim" })
    {
        for (const auto& law : laws)
        {
            SCOPED_TRACE(plant + " " + law.name);
            Session session{ Profiled(plant, law.algorithm) };
            ASSERT_TRUE(session.Settle(0.0f));
            const auto from = session.Mark();
            ASSERT_TRUE(session.mil.SetPosition(move));
            session.Run(450);

            const auto remaining = RemainingReference(session.observed, from);
            const auto duration = Duration(remaining);

            float cruiseError{ 0.0f };
            float peakError{ 0.0f };
            for (std::size_t i = 0; i != remaining.size(); ++i)
            {
                const float error = std::abs(session.observed[from + i].positionError);
                peakError = std::max(peakError, error);
                if (i >= 60 && i <= 80)
                    cruiseError = std::max(cruiseError, error);
            }
            const float finalTargetError = std::abs(session.observed.back().targetError);
            std::fprintf(stderr, "[METRIC] mil profile plant=%s law=%s duration_ms=%zu cruise_error=%.4f peak_error=%.4f final_target_error=%.5f\n",
                plant.c_str(), law.name, duration, cruiseError, peakError, finalTargetError);

            EXPECT_NEAR(static_cast<float>(duration), 125.0f, 1.0f);
            EXPECT_LT(cruiseError, followingErrorLimit);
            EXPECT_LT(finalTargetError, 0.002f);
            EXPECT_TRUE(session.mil.IsRunning());
            mil::ExpectNoDroppedSamples(session.mil);
        }
    }
}

TEST(TestPositionProfile, the_target_is_not_reached_while_the_reference_is_on_its_way)
{
    Session session{ Profiled("nominal", foc::PositionAlgorithm::pid) };
    ASSERT_TRUE(session.Settle(0.0f));
    const auto from = session.Mark();
    ASSERT_TRUE(session.mil.SetPosition(move));
    session.Run(450);

    float smallestWhileMoving{ 10.0f };
    for (std::size_t i = from + 5; i != from + 100; ++i)
        smallestWhileMoving = std::min(smallestWhileMoving, std::abs(session.observed[i].targetError));

    EXPECT_GT(smallestWhileMoving, 0.05f);
}

TEST(TestPositionProfile, a_setpoint_mid_move_continues_from_the_motion_under_way_within_the_limits)
{
    for (const auto& law : laws)
    {
        SCOPED_TRACE(law.name);
        Session session{ Profiled("nominal", law.algorithm) };
        ASSERT_TRUE(session.Settle(0.0f));
        const auto from = session.Mark();
        ASSERT_TRUE(session.mil.SetPosition(move));
        session.Run(40);
        const auto reversal = session.Mark();
        ASSERT_TRUE(session.mil.SetPosition(-1.0f));
        session.Run(500);

        std::vector<float> reference;
        for (std::size_t i = from; i != session.observed.size(); ++i)
        {
            const float target = i < reversal ? move : -1.0f;
            reference.push_back(target - (session.observed[i].targetError - session.observed[i].positionError));
        }

        float peakSpeed{ 0.0f };
        float peakAcceleration{ 0.0f };
        float peakError{ 0.0f };
        for (std::size_t i = 2; i < reference.size(); ++i)
        {
            const float speed = (reference[i] - reference[i - 1]) / periodSeconds;
            const float previous = (reference[i - 1] - reference[i - 2]) / periodSeconds;
            peakSpeed = std::max(peakSpeed, std::abs(speed));
            peakAcceleration = std::max(peakAcceleration, std::abs(speed - previous) / periodSeconds);
            peakError = std::max(peakError, std::abs(session.observed[from + i].positionError));
        }
        std::fprintf(stderr, "[METRIC] mil profile reversal law=%s peak_speed=%.3f peak_acceleration=%.1f peak_error=%.4f final_reference=%.5f\n",
            law.name, peakSpeed, peakAcceleration, peakError, reference.back());

        EXPECT_LE(peakSpeed, profileVelocity * 1.01f);
        EXPECT_LE(peakAcceleration, profileAcceleration * 1.05f);
        EXPECT_LT(peakError, followingErrorLimit);
        EXPECT_NEAR(reference.back(), -1.0f, 1e-3f);
    }
}

TEST(TestPositionProfile, without_a_profile_the_error_to_the_reference_is_the_error_to_the_target)
{
    for (const auto& law : laws)
    {
        SCOPED_TRACE(law.name);
        Session session{ Profiled("nominal", law.algorithm, profileVelocity, 0.0f) };
        ASSERT_TRUE(session.Settle(0.0f));
        const auto from = session.Mark();
        ASSERT_TRUE(session.mil.SetPosition(move));
        session.Run(300);

        float largestDifference{ 0.0f };
        for (std::size_t i = from; i != session.observed.size(); ++i)
            largestDifference = std::max(largestDifference, std::abs(session.observed[i].targetError - session.observed[i].positionError));

        EXPECT_LT(largestDifference, 1e-6f);
        EXPECT_GT(std::abs(session.observed[from + 5].positionError), 0.5f);
    }
}

TEST(TestPositionProfile, every_position_law_moves_1_5_rad_in_145_ms_with_jerk_limit_and_stays_within_cruise_error)
{
    for (const std::string plant : { "nominal", "anaheim" })
    {
        for (const auto& law : laws)
        {
            SCOPED_TRACE(plant + " " + law.name);
            Session session{ Profiled(plant, law.algorithm, profileVelocity, profileAcceleration, profileJerk) };
            ASSERT_TRUE(session.Settle(0.0f));
            const auto from = session.Mark();
            ASSERT_TRUE(session.mil.SetPosition(move));
            session.Run(500);

            const auto remaining = RemainingReference(session.observed, from);
            const auto duration = Duration(remaining);

            float cruiseError{ 0.0f };
            float peakError{ 0.0f };
            for (std::size_t i = 0; i != remaining.size(); ++i)
            {
                const float error = std::abs(session.observed[from + i].positionError);
                peakError = std::max(peakError, error);
                if (i >= 80 && i <= 110)
                    cruiseError = std::max(cruiseError, error);
            }
            const float finalTargetError = std::abs(session.observed.back().targetError);
            std::fprintf(stderr, "[METRIC] mil scurve_profile plant=%s law=%s duration_ms=%zu cruise_error=%.4f peak_error=%.4f final_target_error=%.5f\n",
                plant.c_str(), law.name, duration, cruiseError, peakError, finalTargetError);

            EXPECT_NEAR(static_cast<float>(duration), 142.0f, 2.0f);
            EXPECT_LT(cruiseError, scurveFollowingErrorLimit);
            EXPECT_LT(finalTargetError, 0.002f);
            EXPECT_TRUE(session.mil.IsRunning());
            mil::ExpectNoDroppedSamples(session.mil);
        }
    }
}

TEST(TestPositionProfile, with_jerk_limit_a_mid_move_reversal_keeps_acceleration_rate_within_limit)
{
    for (const auto& law : laws)
    {
        SCOPED_TRACE(law.name);
        Session session{ Profiled("nominal", law.algorithm, profileVelocity, profileAcceleration, profileJerk) };
        ASSERT_TRUE(session.Settle(0.0f));
        const auto from = session.Mark();
        ASSERT_TRUE(session.mil.SetPosition(move));
        session.Run(40);
        const auto reversal = session.Mark();
        ASSERT_TRUE(session.mil.SetPosition(-1.0f));
        session.Run(500);

        std::vector<float> reference;
        for (std::size_t i = from; i != session.observed.size(); ++i)
        {
            const float target = i < reversal ? move : -1.0f;
            reference.push_back(target - (session.observed[i].targetError - session.observed[i].positionError));
        }

        float peakJerkEstimate{ 0.0f };
        for (std::size_t i = 3; i < reference.size(); ++i)
        {
            const float vel2 = (reference[i] - reference[i - 1]) / periodSeconds;
            const float vel1 = (reference[i - 1] - reference[i - 2]) / periodSeconds;
            const float vel0 = (reference[i - 2] - reference[i - 3]) / periodSeconds;
            const float jerk = std::abs((vel2 - 2.0f * vel1 + vel0) / periodSeconds / periodSeconds);
            peakJerkEstimate = std::max(peakJerkEstimate, jerk);
        }
        std::fprintf(stderr, "[METRIC] mil scurve_reversal law=%s peak_jerk_estimate=%.1f\n", law.name, peakJerkEstimate);

        EXPECT_LE(peakJerkEstimate, profileJerk * 1.1f);
        EXPECT_NEAR(reference.back(), -1.0f, 1e-3f);
    }
}
