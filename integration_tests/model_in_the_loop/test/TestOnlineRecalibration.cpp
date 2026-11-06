#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>

namespace
{
    using mil::Quantity;
    using namespace std::chrono_literals;

    mil::Scenario InertiaOffScenario(services::RecalibrationMode mode)
    {
        mil::Scenario scenario;
        scenario.Plant("teknic").Recorded(100, 2000);
        scenario.Calibrated().StoredCalibrationOffBy("inertia", 1.5f).BootsIn(state_machine::ControlMode::speed);
        scenario.nvm.config.recalibrationMode = static_cast<uint8_t>(mode);
        return scenario;
    }

    void RunAndStop(mil::ModelInTheLoop& mil)
    {
        ASSERT_TRUE(mil.Align());
        ASSERT_TRUE(mil.Enable());
        ASSERT_TRUE(mil.AlternateSpeed(26.0f, 52.0f, 250ms, 12000ms));
        ASSERT_TRUE(mil.Disable());
    }

    void ExpectInertiaInUseWithin(mil::ModelInTheLoop& mil, float limitPercent)
    {
        const auto model = mil.ControlMode().ModelInUse();
        ASSERT_TRUE(model.has_value());
        mil::ExpectWithin(mil.Setup(), "recalibrated", Quantity::inertia, model->inertia.Value(), limitPercent);
    }
}

TEST(TestOnlineRecalibration, an_automatic_recalibration_adopts_the_settled_inertia_at_the_stop_and_keeps_the_record)
{
    const auto scenario = InertiaOffScenario(services::RecalibrationMode::automatic);
    mil::ModelInTheLoop mil{ scenario };

    RunAndStop(mil);

    ExpectInertiaInUseWithin(mil, 10.0f);
    EXPECT_GE(mil.ControlMode().RecalibrationsApplied(), 1);
    EXPECT_EQ(0, mil.ControlMode().RecalibrationsRejected());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, mil.CalibrationRecord().inertia, 1e-12f);
    EXPECT_TRUE(mil.IsReady());
}

TEST(TestOnlineRecalibration, with_the_mode_off_the_loops_keep_the_calibration_record)
{
    const auto scenario = InertiaOffScenario(services::RecalibrationMode::off);
    mil::ModelInTheLoop mil{ scenario };

    RunAndStop(mil);

    const auto model = mil.ControlMode().ModelInUse();
    ASSERT_TRUE(model.has_value());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, model->inertia.Value(), 1e-12f);
    EXPECT_EQ(0, mil.ControlMode().RecalibrationsApplied());
    EXPECT_EQ(state_machine::SettingResult::invalidState, mil.ControlMode().CmdRecalibrate());
}

TEST(TestOnlineRecalibration, on_command_the_settled_inertia_is_adopted_only_when_asked)
{
    const auto scenario = InertiaOffScenario(services::RecalibrationMode::onCommand);
    mil::ModelInTheLoop mil{ scenario };

    RunAndStop(mil);
    ASSERT_TRUE(mil.ControlMode().ModelInUse().has_value());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, mil.ControlMode().ModelInUse()->inertia.Value(), 1e-12f);

    EXPECT_EQ(state_machine::SettingResult::ok, mil.ControlMode().CmdRecalibrate());

    ExpectInertiaInUseWithin(mil, 10.0f);
    EXPECT_GE(mil.ControlMode().RecalibrationsApplied(), 1);
}

TEST(TestOnlineRecalibration, the_recalibrated_drive_runs_again_on_the_new_model)
{
    const auto scenario = InertiaOffScenario(services::RecalibrationMode::automatic);
    mil::ModelInTheLoop mil{ scenario };
    RunAndStop(mil);

    ASSERT_TRUE(mil.Enable());
    ASSERT_TRUE(mil.HoldSpeed(40.0f, 1500ms));

    EXPECT_TRUE(mil.IsRunning());
    EXPECT_FALSE(mil.HasFault());
}
