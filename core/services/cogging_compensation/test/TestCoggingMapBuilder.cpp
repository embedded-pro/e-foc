#include "core/services/cogging_compensation/CoggingMapBuilder.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float twoPi{ 2.0f * std::numbers::pi_v<float> };
    constexpr float samplePeriod{ 1.0f / 20000.0f };
    constexpr std::size_t polePairs{ 4 };

    struct Rotor
    {
        float inertia{ 14.0e-6f };
        float torqueConstant{ 0.042f };
        float viscousFriction{ 5.0e-6f };
        float dryFriction{ 3.7e-3f };
        float coggingTorque{ 4.0e-3f };
        float coggingCycles{ 24.0f };
        float countsPerRevolution{ 4000.0f };
    };

    class SpeedLoopRun
    {
    public:
        explicit SpeedLoopRun(const Rotor& rotor)
            : rotor{ rotor }
        {}

        void Turn(services::CoggingMapBuilder& builder, services::CoggingMapBuilder::Direction direction, float speed, float settleTurns, float measuredTurns)
        {
            const auto start = angle;
            bool previousValid{ false };
            float previousMeasured{ 0.0f };

            while (std::abs(angle - start) < twoPi * (settleTurns + measuredTurns))
            {
                const auto current = Step(speed);
                const auto measured = Measured();

                if (std::abs(angle - start) >= twoPi * settleTurns)
                {
                    if (previousValid)
                        builder.Add(direction, measured * static_cast<float>(polePairs), current, measured - previousMeasured);
                    previousValid = true;
                }

                previousMeasured = measured;
            }
        }

        float Cogging(float mechanicalAngle) const
        {
            return rotor.coggingTorque * std::sin(rotor.coggingCycles * mechanicalAngle);
        }

    private:
        float Step(float reference)
        {
            constexpr float bandwidth{ 50.0f };
            const auto proportional = rotor.inertia * bandwidth / rotor.torqueConstant;
            const auto integral = proportional * bandwidth * 0.25f;
            const auto error = reference - speed;

            integrator += integral * error * samplePeriod;
            const auto current = proportional * error + integrator;

            const auto friction = speed > 0.0f ? rotor.dryFriction : (speed < 0.0f ? -rotor.dryFriction : 0.0f);
            const auto acceleration = (rotor.torqueConstant * current - Cogging(angle) - friction - rotor.viscousFriction * speed) / rotor.inertia;
            speed += acceleration * samplePeriod;
            angle += speed * samplePeriod;

            return current;
        }

        float Measured() const
        {
            const auto count = twoPi / rotor.countsPerRevolution;
            return std::round(angle / count) * count;
        }

        Rotor rotor;
        float angle{ 0.0f };
        float speed{ 0.0f };
        float integrator{ 0.0f };
    };

    class TestCoggingMapBuilder
        : public ::testing::Test
    {
    public:
        using Direction = services::CoggingMapBuilder::Direction;

        std::optional<services::CoggingMap> TurnBothWaysAndBuild(float inertiaForBuild)
        {
            SpeedLoopRun run{ rotor };
            run.Turn(builder, Direction::forward, 10.0f, 0.5f, 3.0f);
            run.Turn(builder, Direction::backward, -10.0f, 0.5f, 3.0f);
            return builder.Build(foc::NewtonMeter{ rotor.torqueConstant }, foc::NewtonMeterSecondSquared{ inertiaForBuild }, polePairs, samplePeriod);
        }

        float WorstBinError(const services::CoggingMap& map) const
        {
            SpeedLoopRun reference{ rotor };
            const auto binWidth = twoPi / static_cast<float>(services::coggingMapBins * polePairs);
            constexpr int pointsPerBin{ 16 };

            float worst{ 0.0f };
            for (std::size_t bin = 0; bin != services::coggingMapBins; ++bin)
            {
                float expected{ 0.0f };
                for (int point = 0; point != pointsPerBin; ++point)
                    expected += reference.Cogging((static_cast<float>(bin) + (static_cast<float>(point) + 0.5f) / pointsPerBin) * binWidth) / pointsPerBin;

                worst = std::max(worst, std::abs(static_cast<float>(map.torque[bin]) * map.torqueScale - expected));
            }

            return worst;
        }

        Rotor rotor;
        services::CoggingMapBuilder builder;
    };
}

TEST_F(TestCoggingMapBuilder, a_rotor_carried_through_its_detents_both_ways_yields_its_cogging_without_its_friction)
{
    const auto map = TurnBothWaysAndBuild(rotor.inertia);

    ASSERT_TRUE(map.has_value());
    EXPECT_NEAR(map->peak.Value(), rotor.coggingTorque, 0.1f * rotor.coggingTorque);
    EXPECT_LT(WorstBinError(*map), 0.1f * rotor.coggingTorque);
    EXPECT_GT(map->agreement, services::CoggingMapBuilder::minimumAgreement);
}

TEST_F(TestCoggingMapBuilder, without_the_rotor_inertia_the_two_directions_disagree_and_yield_no_map)
{
    EXPECT_FALSE(TurnBothWaysAndBuild(0.0f).has_value());
}

TEST_F(TestCoggingMapBuilder, a_rotor_that_did_not_cover_every_bin_yields_no_map)
{
    SpeedLoopRun run{ rotor };
    run.Turn(builder, Direction::forward, 10.0f, 0.5f, 0.1f);
    run.Turn(builder, Direction::backward, -10.0f, 0.5f, 3.0f);

    EXPECT_FALSE(builder.Build(foc::NewtonMeter{ rotor.torqueConstant }, foc::NewtonMeterSecondSquared{ rotor.inertia }, polePairs, samplePeriod).has_value());
}

TEST_F(TestCoggingMapBuilder, a_rotor_that_turned_the_wrong_way_for_a_direction_yields_no_map)
{
    SpeedLoopRun run{ rotor };
    run.Turn(builder, Direction::forward, 10.0f, 0.5f, 3.0f);
    run.Turn(builder, Direction::backward, 10.0f, 0.0f, 3.0f);

    EXPECT_FALSE(builder.Build(foc::NewtonMeter{ rotor.torqueConstant }, foc::NewtonMeterSecondSquared{ rotor.inertia }, polePairs, samplePeriod).has_value());
}

namespace
{
    class TestCoggingMapAgreement
        : public ::testing::Test
    {
    public:
        using Direction = services::CoggingMapBuilder::Direction;

        static constexpr float torqueConstant{ 0.042f };
        static constexpr float rotationPerSample{ 1.0e-4f };
        static constexpr int samplesPerRun{ 31416 };

        void Run(Direction direction, float coggingCurrent, float noiseCurrent)
        {
            const auto sign = direction == Direction::forward ? 1.0f : -1.0f;
            for (int sample = 0; sample != samplesPerRun; ++sample)
            {
                const auto electricalAngle = sign * rotationPerSample * static_cast<float>(sample) * static_cast<float>(polePairs);
                const auto current = coggingCurrent * std::sin(6.0f * electricalAngle) + noiseCurrent * Noise();
                builder.Add(direction, electricalAngle, current, sign * rotationPerSample);
            }
        }

        std::optional<services::CoggingMap> Build()
        {
            return builder.Build(foc::NewtonMeter{ torqueConstant }, foc::NewtonMeterSecondSquared{ 14.0e-6f }, polePairs, samplePeriod);
        }

        services::CoggingMapBuilder builder;

    private:
        float Noise()
        {
            state = state * 1664525u + 1013904223u;
            return static_cast<float>(state >> 8) / static_cast<float>(1u << 24) - 0.5f;
        }

        uint32_t state{ 20261022u };
    };
}

TEST_F(TestCoggingMapAgreement, noise_alone_in_both_directions_yields_no_map)
{
    Run(Direction::forward, 0.0f, 1.0f);
    Run(Direction::backward, 0.0f, 1.0f);

    EXPECT_FALSE(Build().has_value());
}

TEST_F(TestCoggingMapAgreement, a_cogging_both_directions_share_yields_a_map_through_the_noise)
{
    Run(Direction::forward, 0.05f, 0.3f);
    Run(Direction::backward, 0.05f, 0.3f);

    const auto map = Build();
    ASSERT_TRUE(map.has_value());
    EXPECT_GT(map->agreement, services::CoggingMapBuilder::minimumAgreement);
    EXPECT_NEAR(map->peak.Value(), 0.05f * torqueConstant, 0.2f * 0.05f * torqueConstant);
}

TEST_F(TestCoggingMapAgreement, a_profile_only_one_direction_shows_yields_no_map)
{
    Run(Direction::forward, 0.05f, 1.0f);
    Run(Direction::backward, 0.0f, 1.0f);

    EXPECT_FALSE(Build().has_value());
}
