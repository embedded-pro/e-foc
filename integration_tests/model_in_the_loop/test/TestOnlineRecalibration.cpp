#include "core/foc/interfaces/MotorModel.hpp"
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
        const auto model = mil.ControlMode().Recalibration().ModelInUse();
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
    EXPECT_GE(mil.ControlMode().Recalibration().RecalibrationsApplied(), 1);
    EXPECT_EQ(0, mil.ControlMode().Recalibration().RecalibrationsRejected());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, mil.CalibrationRecord().inertia, 1e-12f);
    EXPECT_TRUE(mil.IsReady());
}

TEST(TestOnlineRecalibration, with_the_mode_off_the_loops_keep_the_calibration_record)
{
    const auto scenario = InertiaOffScenario(services::RecalibrationMode::off);
    mil::ModelInTheLoop mil{ scenario };

    RunAndStop(mil);

    const auto model = mil.ControlMode().Recalibration().ModelInUse();
    ASSERT_TRUE(model.has_value());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, model->inertia.Value(), 1e-12f);
    EXPECT_EQ(0, mil.ControlMode().Recalibration().RecalibrationsApplied());
    EXPECT_EQ(state_machine::SettingResult::invalidState, mil.ControlMode().Recalibration().CmdRecalibrate());
}

TEST(TestOnlineRecalibration, on_command_the_settled_inertia_is_adopted_only_when_asked)
{
    const auto scenario = InertiaOffScenario(services::RecalibrationMode::onCommand);
    mil::ModelInTheLoop mil{ scenario };

    RunAndStop(mil);
    ASSERT_TRUE(mil.ControlMode().Recalibration().ModelInUse().has_value());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, mil.ControlMode().Recalibration().ModelInUse()->inertia.Value(), 1e-12f);

    EXPECT_EQ(state_machine::SettingResult::ok, mil.ControlMode().Recalibration().CmdRecalibrate());

    ExpectInertiaInUseWithin(mil, 10.0f);
    EXPECT_GE(mil.ControlMode().Recalibration().RecalibrationsApplied(), 1);
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

        std::printf("[METRIC] mil recalibration stores=%u\n", static_cast<unsigned>(mil.ControlMode().Recalibration().RecalibrationsStored()));
        EXPECT_EQ(1, mil.ControlMode().Recalibration().RecalibrationsStored());
        image = mil.NvmImage();
    }

    mil::ModelInTheLoop restarted{ scenario.BootsFrom(image) };
    ASSERT_TRUE(restarted.Align());

    ExpectInertiaInUseWithin(restarted, 10.0f);
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, restarted.CalibrationRecord().inertia, 1e-12f);

    RunAndStop(restarted);
    EXPECT_EQ(0, restarted.ControlMode().Recalibration().RecalibrationsApplied());
    EXPECT_EQ(0, restarted.ControlMode().Recalibration().RecalibrationsStored());
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

    ASSERT_TRUE(restarted.ControlMode().Recalibration().ModelInUse().has_value());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, restarted.ControlMode().Recalibration().ModelInUse()->inertia.Value(), 1e-12f);
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

    EXPECT_EQ(state_machine::SettingResult::ok, restarted.ControlMode().Recalibration().CmdResetModel([&stored](bool result)
                                                    {
                                                        stored = result;
                                                    }));
    restarted.Run(std::chrono::milliseconds{ 10 });

    EXPECT_EQ(true, stored);
    ASSERT_TRUE(restarted.ControlMode().Recalibration().ModelInUse().has_value());
    EXPECT_NEAR(1.5f * scenario.plant.rotorInertiaKgM2, restarted.ControlMode().Recalibration().ModelInUse()->inertia.Value(), 1e-12f);
}

namespace
{
    constexpr float hotWindingCelsius{ 90.0f };
    constexpr float holdSpeed{ 50.0f };
    constexpr float loadTorque{ 0.02f };

    mil::Scenario HotMagnetScenario(services::RecalibrationMode mode)
    {
        mil::Scenario scenario;
        scenario.Plant("hot");
        scenario.plant.loadTorqueNm = loadTorque;
        scenario.Calibrated().BootsIn(state_machine::ControlMode::speed);
        scenario.nvm.config.recalibrationMode = static_cast<uint8_t>(mode);
        return scenario;
    }

    float PlantTorqueConstant(const mil::Scenario& scenario)
    {
        return foc::TorqueConstantFor(scenario.plant.polePairs, foc::Weber{ mil::PlantTruth(scenario.plant, Quantity::fluxLinkage, hotWindingCelsius) }).Value();
    }

    float MeanQuadratureCurrentHoldingSpeed(mil::ModelInTheLoop& mil)
    {
        mil.CurrentStream().Start(mil.TicksFromMilliseconds(1.0f));
        EXPECT_TRUE(mil.Enable());
        EXPECT_TRUE(mil.SetSpeed(holdSpeed));

        float sum{ 0.0f };
        uint32_t count{ 0 };
        for (uint32_t millisecond = 0; millisecond != 2000; ++millisecond)
        {
            mil.Run(std::chrono::milliseconds{ 1 });
            while (const auto sample = mil.CurrentStream().Take())
                if (millisecond >= 1700)
                {
                    sum += sample->meanIq.Value();
                    ++count;
                }
        }

        EXPECT_GT(count, 0u);
        return count != 0 ? sum / static_cast<float>(count) : 0.0f;
    }

    float TorqueBalanceErrorPercent(mil::ModelInTheLoop& mil, const mil::Scenario& scenario)
    {
        const auto model = mil.ControlMode().Recalibration().ModelInUse();
        EXPECT_TRUE(model.has_value());
        const auto torqueConstant = foc::TorqueConstantFor(scenario.plant.polePairs, model->fluxLinkage).Value();
        const auto demanded = torqueConstant * MeanQuadratureCurrentHoldingSpeed(mil);
        const auto required = loadTorque + scenario.plant.viscousDampingNmSPerRad * holdSpeed;
        return std::fabs(demanded - required) / required * 100.0f;
    }
}

TEST(TestOnlineRecalibration, an_automatic_recalibration_adopts_the_hot_magnets_flux_linkage_and_keeps_the_record)
{
    const auto scenario = HotMagnetScenario(services::RecalibrationMode::automatic);
    mil::ModelInTheLoop mil{ scenario };

    RunAndStop(mil);

    const auto model = mil.ControlMode().Recalibration().ModelInUse();
    ASSERT_TRUE(model.has_value());
    mil::ExpectWithin(mil.Setup(), "recalibrated", Quantity::fluxLinkage, model->fluxLinkage.Value(), 1.0f, hotWindingCelsius);
    EXPECT_NEAR(scenario.plant.fluxLinkageWeber, mil.CalibrationRecord().fluxLinkage, 1e-9f);
    EXPECT_NEAR(scenario.plant.fluxLinkageWeber, mil.ControlMode().Drive().ActiveCommon().Calibration().EffectiveFluxLinkage().Value(), 1e-9f);
    EXPECT_EQ(0, mil.ControlMode().Recalibration().RecalibrationsRejected());
}

TEST(TestOnlineRecalibration, the_recalibrated_flux_linkage_restores_the_torque_per_ampere_of_a_hot_magnet)
{
    const auto scenario = HotMagnetScenario(services::RecalibrationMode::automatic);
    mil::ModelInTheLoop mil{ scenario };
    RunAndStop(mil);

    const auto errorPercent = TorqueBalanceErrorPercent(mil, scenario);

    std::printf("[METRIC] mil recalibration torque balance after=%.2f%% hot_magnet_kt_loss=%.2f%%\n", errorPercent,
        (1.0f - PlantTorqueConstant(scenario) / foc::TorqueConstantFor(scenario.plant.polePairs, foc::Weber{ scenario.plant.fluxLinkageWeber }).Value()) * 100.0f);
    EXPECT_LE(errorPercent, 1.0f);
}

TEST(TestOnlineRecalibration, with_the_mode_off_the_hot_magnet_keeps_the_calibration_records_flux_linkage)
{
    const auto scenario = HotMagnetScenario(services::RecalibrationMode::off);
    mil::ModelInTheLoop mil{ scenario };

    RunAndStop(mil);

    const auto model = mil.ControlMode().Recalibration().ModelInUse();
    ASSERT_TRUE(model.has_value());
    EXPECT_NEAR(scenario.plant.fluxLinkageWeber, model->fluxLinkage.Value(), 1e-9f);
    EXPECT_NEAR(scenario.plant.fluxLinkageWeber, mil.ControlMode().Drive().ActiveCommon().Calibration().ActiveFluxLinkage().Value(), 1e-9f);
    EXPECT_GT(TorqueBalanceErrorPercent(mil, scenario), 5.0f);
}

TEST(TestOnlineRecalibration, on_command_the_hot_magnets_flux_linkage_is_adopted_only_when_asked)
{
    const auto scenario = HotMagnetScenario(services::RecalibrationMode::onCommand);
    mil::ModelInTheLoop mil{ scenario };
    RunAndStop(mil);
    ASSERT_TRUE(mil.ControlMode().Recalibration().ModelInUse().has_value());
    EXPECT_NEAR(scenario.plant.fluxLinkageWeber, mil.ControlMode().Recalibration().ModelInUse()->fluxLinkage.Value(), 1e-9f);

    EXPECT_EQ(state_machine::SettingResult::ok, mil.ControlMode().Recalibration().CmdRecalibrate());

    mil::ExpectWithin(mil.Setup(), "recalibrated", Quantity::fluxLinkage, mil.ControlMode().Recalibration().ModelInUse()->fluxLinkage.Value(), 1.0f, hotWindingCelsius);
}

TEST(TestOnlineRecalibration, torque_mode_keeps_the_flux_linkage_a_speed_mode_recalibration_adopted)
{
    const auto scenario = HotMagnetScenario(services::RecalibrationMode::automatic);
    mil::ModelInTheLoop mil{ scenario };
    RunAndStop(mil);

    ASSERT_TRUE(mil.Select(state_machine::ControlMode::torque));
    mil.Run(100ms);

    const auto inUse = mil.ControlMode().Drive().ActiveCommon().Calibration().ActiveFluxLinkage().Value();
    mil::ExpectWithin(mil.Setup(), "torque mode", Quantity::fluxLinkage, inUse, 1.0f, hotWindingCelsius);
    EXPECT_NEAR(scenario.plant.fluxLinkageWeber, mil.CalibrationRecord().fluxLinkage, 1e-9f);
    EXPECT_NEAR(foc::TorqueConstantFor(scenario.plant.polePairs, foc::Weber{ inUse }).Value(), PlantTorqueConstant(scenario), 0.01f * PlantTorqueConstant(scenario));
    EXPECT_TRUE(mil.IsReady());
}

TEST(TestOnlineRecalibration, torque_mode_returns_to_the_records_flux_linkage_when_the_model_is_reset)
{
    const auto scenario = HotMagnetScenario(services::RecalibrationMode::automatic);
    mil::ModelInTheLoop mil{ scenario };
    RunAndStop(mil);
    ASSERT_TRUE(mil.Select(state_machine::ControlMode::torque));
    mil.Run(100ms);
    ASSERT_LT(mil.ControlMode().Drive().ActiveCommon().Calibration().ActiveFluxLinkage().Value(), 0.95f * scenario.plant.fluxLinkageWeber);

    std::optional<bool> stored;
    EXPECT_EQ(state_machine::SettingResult::ok, mil.ControlMode().Recalibration().CmdResetModel([&stored](bool result)
                                                    {
                                                        stored = result;
                                                    }));
    mil.Run(10ms);

    EXPECT_EQ(true, stored);
    EXPECT_NEAR(scenario.plant.fluxLinkageWeber, mil.ControlMode().Drive().ActiveCommon().Calibration().ActiveFluxLinkage().Value(), 1e-9f);
}
