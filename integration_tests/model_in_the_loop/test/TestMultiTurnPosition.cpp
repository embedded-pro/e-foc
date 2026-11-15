#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <numbers>
#include <optional>
#include <string>

namespace
{
    constexpr float turn{ 2.0f * std::numbers::pi_v<float> };
    constexpr float profileVelocity{ 60.0f };
    constexpr float profileAcceleration{ 600.0f };
    constexpr std::chrono::milliseconds moveTime{ 1200 };
    constexpr float travelTolerance{ 0.02f };
    constexpr float positionTolerance{ 0.005f };

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

    mil::Scenario Profiled(const std::string& plant, foc::PositionAlgorithm algorithm, bool circular = false)
    {
        auto scenario = mil::Scenario{}.Plant(plant).PositionLoop(algorithm).Recorded(100, 4000).Calibrated().BootsIn(state_machine::ControlMode::position);
        scenario.nvm.config.limits.profileVelocity = profileVelocity;
        scenario.nvm.config.limits.profileAcceleration = profileAcceleration;
        scenario.nvm.config.positionCircular = circular ? 1 : 0;
        return scenario;
    }

    std::optional<float> SignedTravelSinceSetpoint(mil::ModelInTheLoop& mil)
    {
        const auto setpointTick = mil.SetpointTick();
        if (!setpointTick)
            return std::nullopt;

        std::optional<float> first;
        float last{ 0.0f };
        for (const auto& sample : mil.Trace().Samples())
            if (sample.tick >= *setpointTick)
            {
                if (!first)
                    first = sample.thetaMech;
                last = sample.thetaMech;
            }

        if (!first)
            return std::nullopt;
        return last - *first;
    }

    float MeasuredPosition(mil::ModelInTheLoop& mil)
    {
        return mil.ControlMode().Drive().ActiveStateMachine().ObserveMotion().measuredPosition.Value();
    }

    void Move(mil::ModelInTheLoop& mil, float setpoint, float expectedTravel, const std::string& label)
    {
        ASSERT_TRUE(mil.SetPosition(setpoint));
        mil.Run(moveTime);

        const auto travel = SignedTravelSinceSetpoint(mil);
        ASSERT_TRUE(travel.has_value());
        const auto measured = MeasuredPosition(mil);
        std::fprintf(stderr, "[METRIC] mil multi_turn %s setpoint=%.4f travel=%.4f measured=%.4f\n", label.c_str(), setpoint, *travel, measured);

        EXPECT_NEAR(*travel, expectedTravel, travelTolerance) << label;
        EXPECT_NEAR(measured, setpoint, positionTolerance) << label;
    }
}

TEST(TestMultiTurnPosition, every_position_law_turns_the_rotor_whole_turns_in_the_commanded_direction)
{
    for (const std::string plant : { "nominal", "anaheim" })
        for (const auto& law : laws)
        {
            const auto label = plant + " " + law.name;
            SCOPED_TRACE(label);
            mil::ModelInTheLoop mil{ Profiled(plant, law.algorithm) };
            ASSERT_TRUE(mil.Align());
            ASSERT_TRUE(mil.SetPosition(0.0f));
            ASSERT_TRUE(mil.Enable());
            mil.Run(std::chrono::milliseconds{ 300 });

            Move(mil, 3.0f * turn, 3.0f * turn, label + " forward");
            Move(mil, -turn, -4.0f * turn, label + " back");

            EXPECT_TRUE(mil.IsRunning());
            mil::ExpectNoDroppedSamples(mil);
        }
}

TEST(TestMultiTurnPosition, the_turns_counted_while_stopped_carry_into_the_next_start)
{
    mil::ModelInTheLoop mil{ Profiled("nominal", foc::PositionAlgorithm::pid) };
    ASSERT_TRUE(mil.Align());
    ASSERT_TRUE(mil.SetPosition(0.0f));
    ASSERT_TRUE(mil.Enable());
    mil.Run(std::chrono::milliseconds{ 300 });
    Move(mil, 2.0f * turn, 2.0f * turn, "before the stop");

    ASSERT_TRUE(mil.Disable());
    mil.Run(std::chrono::milliseconds{ 200 });
    ASSERT_TRUE(mil.Enable());
    mil.Run(std::chrono::milliseconds{ 300 });

    EXPECT_NEAR(MeasuredPosition(mil), 2.0f * turn, positionTolerance);
    Move(mil, 0.0f, -2.0f * turn, "after the stop");
}

TEST(TestMultiTurnPosition, in_circular_mode_a_setpoint_turns_away_is_reached_the_shorter_way_round)
{
    mil::ModelInTheLoop mil{ Profiled("nominal", foc::PositionAlgorithm::pid, true) };
    ASSERT_TRUE(mil.Align());
    ASSERT_TRUE(mil.SetPosition(0.0f));
    ASSERT_TRUE(mil.Enable());
    mil.Run(std::chrono::milliseconds{ 300 });

    ASSERT_TRUE(mil.SetPosition(3.0f * turn + 0.5f));
    mil.Run(moveTime);

    const auto travel = SignedTravelSinceSetpoint(mil);
    ASSERT_TRUE(travel.has_value());
    std::fprintf(stderr, "[METRIC] mil multi_turn circular travel=%.4f measured=%.4f\n", *travel, MeasuredPosition(mil));
    EXPECT_NEAR(*travel, 0.5f, travelTolerance);
    EXPECT_NEAR(MeasuredPosition(mil), 0.5f, positionTolerance);
}
