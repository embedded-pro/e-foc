#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <array>
#include <gtest/gtest.h>

namespace
{
    using mil::positionLoop;
    using mil::speedLoop;
    using mil::StepRow;

    struct PlantRow
    {
        const char* preset;
        StepRow envelope;
    };

    class TestControlRobustness
        : public ::testing::Test
    {};
}

TEST_F(TestControlRobustness, the_speed_loop_holds_its_envelope_on_off_nominal_plants)
{
    static constexpr std::array<PlantRow, 4> rows{ {
        { "noisy", { "noisy", 20.0f, 10.0f, 300.0f, 60.0f, 20.0f, 10.0f, 2.0f } },
        { "hot", { "hot", 20.0f, 10.0f, 300.0f, 60.0f, 20.0f, 6.0f, 2.0f } },
        { "loaded", { "mechanically loaded", 20.0f, 10.0f, 300.0f, 60.0f, 400.0f, 10.0f, 2.0f } },
        { "anaheim", { "differently wound", 20.0f, 10.0f, 300.0f, 60.0f, 20.0f, 5.0f, 2.0f } },
    } };

    for (const auto& row : rows)
        mil::StepFromRest(mil::Scenario{}.Plant(row.preset), speedLoop, row.envelope);
}

TEST_F(TestControlRobustness, the_position_loop_holds_its_envelope_on_off_nominal_plants)
{
    static constexpr std::array<PlantRow, 4> rows{ {
        { "noisy", { "noisy", 1.5f, 10.0f, 400.0f, 60.0f, 190.0f, 8.0f, 0.3f } },
        { "hot", { "hot", 1.5f, 10.0f, 400.0f, 60.0f, 185.0f, 8.0f, 0.3f } },
        { "loaded", { "mechanically loaded", 1.5f, 10.0f, 400.0f, 60.0f, 340.0f, 13.0f, 0.3f } },
        { "anaheim", { "differently wound", 1.5f, 10.0f, 400.0f, 60.0f, 185.0f, 8.0f, 0.3f } },
    } };

    for (const auto& row : rows)
        mil::StepFromRest(mil::Scenario{}.Plant(row.preset), positionLoop, row.envelope);
}

TEST_F(TestControlRobustness, every_speed_law_stays_bounded_on_the_anaheim_motor)
{
    static constexpr std::array<std::pair<foc::SpeedAlgorithm, StepRow>, 3> rows{ {
        { foc::SpeedAlgorithm::lqi, { "lqi", 20.0f, 10.0f, 300.0f, 60.0f, 30.0f, 5.0f, 2.0f } },
        { foc::SpeedAlgorithm::adrc, { "adrc", 20.0f, 10.0f, 300.0f, 60.0f, 40.0f, 5.0f, 2.0f } },
        { foc::SpeedAlgorithm::twoDof, { "twodof", 20.0f, 10.0f, 300.0f, 60.0f, 30.0f, 4.0f, 2.0f } },
    } };

    for (const auto& [algorithm, row] : rows)
        mil::StepFromRest(mil::Scenario{}.Plant("anaheim").SpeedLoop(algorithm), speedLoop, row);
}

TEST_F(TestControlRobustness, every_position_law_stays_bounded_on_the_anaheim_motor)
{
    static constexpr std::array<std::pair<foc::PositionAlgorithm, StepRow>, 4> rows{ {
        { foc::PositionAlgorithm::cascadeP, { "cascadep", 1.5f, 10.0f, 400.0f, 60.0f, 210.0f, 1.0f, 0.3f } },
        { foc::PositionAlgorithm::lqr, { "lqr", 1.5f, 10.0f, 400.0f, 60.0f, 30.0f, 1.0f, 0.3f } },
        { foc::PositionAlgorithm::lqi, { "lqi", 1.5f, 10.0f, 400.0f, 60.0f, 12.0f, 1.0f, 0.3f } },
        { foc::PositionAlgorithm::twoDof, { "twodof", 1.5f, 10.0f, 400.0f, 60.0f, 310.0f, 8.0f, 0.3f } },
    } };

    for (const auto& [algorithm, row] : rows)
        mil::StepFromRest(mil::Scenario{}.Plant("anaheim").PositionLoop(algorithm), positionLoop, row);
}
