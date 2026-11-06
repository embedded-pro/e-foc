#include "integration_tests/model_in_the_loop/test/TestSupport.hpp"
#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

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

TEST(TestOnlineRecalibration, a_recalibrated_inertia_is_stored_once_and_in_use_after_a_restart)
{
    auto scenario = InertiaOffScenario(services::RecalibrationMode::automatic);
    std::vector<uint8_t> image;
    {
        mil::ModelInTheLoop mil{ scenario };
        RunAndStop(mil);
        RunAndStop(mil);
        RunAndStop(mil);

        std::printf("[METRIC] mil recalibration stores=%u\n", static_cast<unsigned>(mil.ControlMode().RecalibrationsStored()));
        EXPECT_EQ(1, mil.ControlMode().RecalibrationsStored());
        image = mil.NvmImage();
    }

    mil::ModelInTheLoop restarted{ scenario.BootsFrom(image) };
    ASSERT_TRUE(restarted.Align());

    ExpectInertiaInUseWithin(restarted, 10.0f);
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, restarted.CalibrationRecord().inertia, 1e-12f);

    RunAndStop(restarted);
    EXPECT_EQ(0, restarted.ControlMode().RecalibrationsApplied());
    EXPECT_EQ(0, restarted.ControlMode().RecalibrationsStored());
}

TEST(TestOnlineRecalibration, turning_the_mode_off_returns_a_restored_drive_to_the_calibration_record_at_the_next_stop)
{
    auto scenario = InertiaOffScenario(services::RecalibrationMode::automatic);
    std::vector<uint8_t> image;
    {
        mil::ModelInTheLoop mil{ scenario };
        RunAndStop(mil);
        image = mil.NvmImage();
    }

    mil::ModelInTheLoop restarted{ scenario.BootsFrom(image) };
    restarted.ControlMode().Config().recalibrationMode = static_cast<uint8_t>(services::RecalibrationMode::off);
    ASSERT_TRUE(restarted.Align());

    ASSERT_TRUE(restarted.ControlMode().ModelInUse().has_value());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, restarted.ControlMode().ModelInUse()->inertia.Value(), 1e-12f);
}

TEST(TestOnlineRecalibration, a_reset_model_returns_the_restarted_drive_to_the_calibration_record)
{
    auto scenario = InertiaOffScenario(services::RecalibrationMode::automatic);
    std::vector<uint8_t> image;
    {
        mil::ModelInTheLoop mil{ scenario };
        RunAndStop(mil);
        image = mil.NvmImage();
    }
    mil::ModelInTheLoop restarted{ scenario.BootsFrom(image) };
    std::optional<bool> stored;

    EXPECT_EQ(state_machine::SettingResult::ok, restarted.ControlMode().CmdResetModel([&stored](bool result)
                                                    {
                                                        stored = result;
                                                    }));
    restarted.Run(std::chrono::milliseconds{ 10 });

    EXPECT_EQ(true, stored);
    ASSERT_TRUE(restarted.ControlMode().ModelInUse().has_value());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, restarted.ControlMode().ModelInUse()->inertia.Value(), 1e-12f);
}
