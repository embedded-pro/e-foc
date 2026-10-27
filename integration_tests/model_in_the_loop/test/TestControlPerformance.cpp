#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <array>
#include <gtest/gtest.h>

namespace
{
    using integration::response::Signal;
    using mil::currentLoop;
    using mil::Loop;
    using mil::positionLoop;
    using mil::speedLoop;
    using mil::StepRow;

    class TestControlPerformance
        : public ::testing::Test
    {
    protected:
        void StepWhileRunning(mil::Scenario scenario, const Loop& loop, uint32_t sampleRateHz, uint32_t maxSamples, float captureAfterEnableMs, float captureAfterSetpointMs, const StepRow& row)
        {
            SCOPED_TRACE(row.label);
            scenario.Recorded(sampleRateHz, maxSamples).Calibrated().BootsIn(loop.mode);

            mil::ModelInTheLoop mil{ scenario };
            ASSERT_TRUE(mil.Align());
            EXPECT_TRUE(mil::RunsSelectedAlgorithm(mil, loop.signal));
            ASSERT_TRUE(mil::Apply(mil, loop.signal, loop.initial));
            ASSERT_TRUE(mil.Enable());
            ASSERT_TRUE(mil.CaptureAfterEnable(captureAfterEnableMs));
            ASSERT_TRUE(mil::Apply(mil, loop.signal, row.target));
            ASSERT_TRUE(mil.CaptureAfterSetpoint(captureAfterSetpointMs));

            mil::ExpectStepEnvelope(mil::MeasureStep(mil, loop.signal, row.bandPercent, row.label), row.settleMs, row.overshootPercent, row.riseMs, row.tailPercent, row.steadyStateError);
            mil::ExpectNoDroppedSamples(mil);
        }
    };
}

TEST_F(TestControlPerformance, every_speed_law_steps_from_rest_to_20_rad_per_s)
{
    static constexpr std::array<std::pair<foc::SpeedAlgorithm, StepRow>, 4> rows{ {
        { foc::SpeedAlgorithm::pid, { "pid", 20.0f, 10.0f, 60.0f, 15.0f, 20.0f, 6.0f, 0.5f } },
        { foc::SpeedAlgorithm::adrc, { "adrc", 20.0f, 10.0f, 80.0f, 15.0f, 50.0f, 6.0f, 0.5f } },
        { foc::SpeedAlgorithm::twoDof, { "twodof", 20.0f, 10.0f, 60.0f, 15.0f, 30.0f, 6.0f, 0.5f } },
        { foc::SpeedAlgorithm::lqi, { "lqi", 20.0f, 10.0f, 60.0f, 15.0f, 25.0f, 6.0f, 0.5f } },
    } };

    for (const auto& [algorithm, row] : rows)
        mil::StepFromRest(mil::Scenario{}.SpeedLoop(algorithm), speedLoop, row);
}

TEST_F(TestControlPerformance, every_speed_law_follows_a_setpoint_change_while_running)
{
    static constexpr std::array<std::pair<foc::SpeedAlgorithm, StepRow>, 8> rows{ {
        { foc::SpeedAlgorithm::pid, { "pid to 40", 40.0f, 10.0f, 60.0f, 15.0f, 25.0f, 5.0f, 0.5f } },
        { foc::SpeedAlgorithm::pid, { "pid to -20", -20.0f, 10.0f, 60.0f, 15.0f, 20.0f, 4.0f, 0.5f } },
        { foc::SpeedAlgorithm::adrc, { "adrc to 40", 40.0f, 10.0f, 80.0f, 15.0f, 50.0f, 6.0f, 0.5f } },
        { foc::SpeedAlgorithm::adrc, { "adrc to -20", -20.0f, 10.0f, 80.0f, 15.0f, 40.0f, 4.0f, 0.5f } },
        { foc::SpeedAlgorithm::twoDof, { "twodof to 40", 40.0f, 10.0f, 60.0f, 15.0f, 40.0f, 6.0f, 0.5f } },
        { foc::SpeedAlgorithm::twoDof, { "twodof to -20", -20.0f, 10.0f, 60.0f, 15.0f, 30.0f, 4.0f, 0.5f } },
        { foc::SpeedAlgorithm::lqi, { "lqi to 40", 40.0f, 10.0f, 60.0f, 15.0f, 35.0f, 6.0f, 0.5f } },
        { foc::SpeedAlgorithm::lqi, { "lqi to -20", -20.0f, 10.0f, 60.0f, 15.0f, 30.0f, 4.0f, 0.5f } },
    } };

    for (const auto& [algorithm, row] : rows)
        StepWhileRunning(mil::Scenario{}.SpeedLoop(algorithm), speedLoop, 1000, 2000, 400.0f, 550.0f, row);
}

TEST_F(TestControlPerformance, every_position_law_steps_from_rest_to_1_5_rad)
{
    static constexpr std::array<std::pair<foc::PositionAlgorithm, StepRow>, 5> rows{ {
        { foc::PositionAlgorithm::pid, { "pid", 1.5f, 10.0f, 200.0f, 15.0f, 185.0f, 8.0f, 0.1f } },
        { foc::PositionAlgorithm::cascadeP, { "cascadep", 1.5f, 10.0f, 200.0f, 10.0f, 210.0f, 1.0f, 0.02f } },
        { foc::PositionAlgorithm::lqr, { "lqr", 1.5f, 10.0f, 60.0f, 15.0f, 30.0f, 1.0f, 0.01f } },
        { foc::PositionAlgorithm::lqi, { "lqi", 1.5f, 10.0f, 60.0f, 50.0f, 10.0f, 1.0f, 0.01f } },
        { foc::PositionAlgorithm::twoDof, { "twodof", 1.5f, 10.0f, 300.0f, 15.0f, 310.0f, 8.0f, 0.1f } },
    } };

    for (const auto& [algorithm, row] : rows)
        mil::StepFromRest(mil::Scenario{}.PositionLoop(algorithm), positionLoop, row);
}

TEST_F(TestControlPerformance, every_position_law_follows_a_setpoint_change_while_holding)
{
    static constexpr std::array<std::pair<foc::PositionAlgorithm, StepRow>, 5> rows{ {
        { foc::PositionAlgorithm::pid, { "pid", -1.5f, 10.0f, 200.0f, 15.0f, 195.0f, 6.0f, 0.15f } },
        { foc::PositionAlgorithm::cascadeP, { "cascadep", -1.5f, 10.0f, 200.0f, 10.0f, 210.0f, 1.0f, 0.02f } },
        { foc::PositionAlgorithm::lqr, { "lqr", -1.5f, 10.0f, 60.0f, 15.0f, 30.0f, 1.0f, 0.01f } },
        { foc::PositionAlgorithm::lqi, { "lqi", -1.5f, 10.0f, 60.0f, 40.0f, 15.0f, 1.0f, 0.01f } },
        { foc::PositionAlgorithm::twoDof, { "twodof", -1.5f, 10.0f, 300.0f, 15.0f, 320.0f, 6.0f, 0.15f } },
    } };

    for (const auto& [algorithm, row] : rows)
        StepWhileRunning(mil::Scenario{}.PositionLoop(algorithm), positionLoop, 1000, 2000, 400.0f, 550.0f, row);
}

TEST_F(TestControlPerformance, every_current_law_steps_from_rest_to_0_5_A)
{
    static constexpr std::array<std::pair<foc::CurrentAlgorithm, StepRow>, 4> rows{ {
        { foc::CurrentAlgorithm::pid, { "pid", 0.5f, 25.0f, 3.0f, 30.0f, 15.0f, 35.0f, 0.12f } },
        { foc::CurrentAlgorithm::decoupledPid, { "decoupled", 0.5f, 10.0f, 3.0f, 30.0f, 4.0f, 1.0f, 0.05f } },
        { foc::CurrentAlgorithm::deadbeat, { "deadbeat", 0.5f, 10.0f, 1.0f, 30.0f, 1.0f, 7.0f, 0.05f } },
        { foc::CurrentAlgorithm::slidingMode, { "sliding", 0.5f, 10.0f, 3.0f, 30.0f, 1.0f, 13.0f, 0.05f } },
    } };

    for (const auto& [algorithm, row] : rows)
        mil::StepFromRest(mil::Scenario{}.CurrentLoop(algorithm), currentLoop, row);
}

TEST_F(TestControlPerformance, the_pid_current_law_follows_a_setpoint_change_while_running)
{
    StepWhileRunning(mil::Scenario{}.CurrentLoop(foc::CurrentAlgorithm::pid), currentLoop, 5000, 10000, 15.0f, 60.0f,
        { "pid to -0.5", -0.5f, 40.0f, 12.0f, 30.0f, 6.0f, 65.0f, 0.2f });
}
