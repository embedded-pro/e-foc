#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <array>
#include <gtest/gtest.h>

namespace
{
    using integration::response::Signal;

    struct DisturbanceRow
    {
        const char* label;
        float torqueNm;
        float deviation;
        float band;
        float recoveryMs;
    };

    class TestDisturbanceRejection
        : public ::testing::Test
    {
    protected:
        void Reject(mil::Scenario scenario, Signal signal, float setpoint, uint32_t maxSamples, uint32_t torqueDelayMs, float captureMs, const DisturbanceRow& row)
        {
            SCOPED_TRACE(row.label);
            const auto mode = signal == Signal::speed ? state_machine::ControlMode::speed : state_machine::ControlMode::position;
            scenario.Recorded(1000, maxSamples).TorqueStep(row.torqueNm, torqueDelayMs).Calibrated().BootsIn(mode);

            mil::ModelInTheLoop mil{ scenario };
            ASSERT_TRUE(mil.Align());
            ASSERT_TRUE(signal == Signal::speed ? mil.SetSpeed(setpoint) : mil.SetPosition(setpoint));
            ASSERT_TRUE(mil.Enable());
            ASSERT_TRUE(mil.CaptureAfterEnable(captureMs));

            const auto excursion = mil::MeasureDisturbance(mil, signal, row.deviation);
            ASSERT_TRUE(excursion.has_value());
            EXPECT_LT(excursion->maxDeviation, row.deviation);

            const auto recovery = mil::MeasureDisturbance(mil, signal, row.band);
            ASSERT_TRUE(recovery.has_value());
            EXPECT_LE(recovery->recoveryTimeS * mil::milliPerUnit, row.recoveryMs);
            EXPECT_LT(std::fabs(recovery->steadyStateError), row.band);

            mil::ExpectNoDroppedSamples(mil);
        }

        void RejectWhileHoldingSpeed(foc::SpeedAlgorithm algorithm, const DisturbanceRow& row)
        {
            Reject(mil::Scenario{}.SpeedLoop(algorithm), Signal::speed, 20.0f, 1200, 300, 750.0f, row);
        }

        void RejectWhileHoldingPosition(foc::PositionAlgorithm algorithm, const DisturbanceRow& row)
        {
            Reject(mil::Scenario{}.PositionLoop(algorithm), Signal::position, 1.5f, 1400, 500, 950.0f, row);
        }
    };
}

TEST_F(TestDisturbanceRejection, every_speed_law_rejects_a_torque_step_of_either_sign_while_holding_20_rad_per_s)
{
    static constexpr std::array<std::pair<foc::SpeedAlgorithm, DisturbanceRow>, 8> rows{ {
        { foc::SpeedAlgorithm::pid, { "pid +", 0.002f, 4.0f, 2.0f, 50.0f } },
        { foc::SpeedAlgorithm::pid, { "pid -", -0.002f, 4.0f, 2.0f, 50.0f } },
        { foc::SpeedAlgorithm::adrc, { "adrc +", 0.002f, 4.0f, 2.0f, 50.0f } },
        { foc::SpeedAlgorithm::adrc, { "adrc -", -0.002f, 4.0f, 2.0f, 50.0f } },
        { foc::SpeedAlgorithm::twoDof, { "twodof +", 0.002f, 4.0f, 2.0f, 50.0f } },
        { foc::SpeedAlgorithm::twoDof, { "twodof -", -0.002f, 4.0f, 2.0f, 50.0f } },
        { foc::SpeedAlgorithm::lqi, { "lqi +", 0.002f, 4.0f, 2.0f, 50.0f } },
        { foc::SpeedAlgorithm::lqi, { "lqi -", -0.002f, 4.0f, 2.0f, 50.0f } },
    } };

    for (const auto& [algorithm, row] : rows)
        RejectWhileHoldingSpeed(algorithm, row);
}

TEST_F(TestDisturbanceRejection, every_position_law_holds_1_5_rad_against_a_torque_step_of_either_sign)
{
    static constexpr std::array<std::pair<foc::PositionAlgorithm, DisturbanceRow>, 10> rows{ {
        { foc::PositionAlgorithm::pid, { "pid +", 0.002f, 0.1f, 0.08f, 50.0f } },
        { foc::PositionAlgorithm::pid, { "pid -", -0.002f, 0.1f, 0.08f, 50.0f } },
        { foc::PositionAlgorithm::cascadeP, { "cascadep +", 0.002f, 0.1f, 0.08f, 50.0f } },
        { foc::PositionAlgorithm::cascadeP, { "cascadep -", -0.002f, 0.1f, 0.08f, 50.0f } },
        { foc::PositionAlgorithm::lqr, { "lqr +", 0.002f, 0.1f, 0.08f, 50.0f } },
        { foc::PositionAlgorithm::lqr, { "lqr -", -0.002f, 0.1f, 0.08f, 50.0f } },
        { foc::PositionAlgorithm::lqi, { "lqi +", 0.002f, 0.05f, 0.02f, 50.0f } },
        { foc::PositionAlgorithm::lqi, { "lqi -", -0.002f, 0.05f, 0.02f, 50.0f } },
        { foc::PositionAlgorithm::twoDof, { "twodof +", 0.002f, 0.1f, 0.08f, 50.0f } },
        { foc::PositionAlgorithm::twoDof, { "twodof -", -0.002f, 0.1f, 0.08f, 50.0f } },
    } };

    for (const auto& [algorithm, row] : rows)
        RejectWhileHoldingPosition(algorithm, row);
}

TEST_F(TestDisturbanceRejection, every_speed_law_recovers_through_a_saturated_current_command)
{
    static constexpr std::array<std::pair<foc::SpeedAlgorithm, DisturbanceRow>, 4> rows{ {
        { foc::SpeedAlgorithm::pid, { "pid", 0.5f, 300.0f, 3.0f, 350.0f } },
        { foc::SpeedAlgorithm::adrc, { "adrc", 0.5f, 300.0f, 3.0f, 350.0f } },
        { foc::SpeedAlgorithm::twoDof, { "twodof", 0.5f, 300.0f, 3.0f, 350.0f } },
        { foc::SpeedAlgorithm::lqi, { "lqi", 0.5f, 300.0f, 3.0f, 350.0f } },
    } };

    for (const auto& [algorithm, row] : rows)
        RejectWhileHoldingSpeed(algorithm, row);
}
