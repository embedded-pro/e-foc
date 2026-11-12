#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <optional>

namespace
{
    constexpr float supplyVolts{ 24.0f };

    class TestPlantNonidealities
        : public ::testing::Test
    {
    protected:
        void DriveCycles(int cycles, int percentA, int percentB, int percentC)
        {
            const foc::PhasePwmDutyCycles duty{
                hal::DutyCycle::FromPercent(percentA),
                hal::DutyCycle::FromPercent(percentB),
                hal::DutyCycle::FromPercent(percentC)
            };
            for (int i = 0; i < cycles; ++i)
                model.StepForTest(duty);
        }

        void DriveHard(int cycles)
        {
            DriveCycles(cycles, 90, 50, 10);
        }

        foc::ThreePhaseMotorModel model{
            foc::M_2310P_LN_04K::parameters,
            foc::Volts{ supplyVolts },
            hal::Hertz{ 20000 }
        };
    };
}

TEST_F(TestPlantNonidealities, an_ideal_supply_keeps_its_voltage_under_load)
{
    model.Start();
    DriveHard(2000);

    EXPECT_NEAR(model.Supply().Voltage().Value(), supplyVolts, 1e-6f);
}

TEST_F(TestPlantNonidealities, source_resistance_pulls_the_bus_down_while_the_motor_draws_current)
{
    model.Supply().Configure({ .sourceResistanceOhm = 1.0f });
    model.Start();
    DriveHard(2000);

    EXPECT_GT(model.Supply().Current().Value(), 0.0f);
    EXPECT_LT(model.Supply().Voltage().Value(), supplyVolts - 0.1f);
}

TEST_F(TestPlantNonidealities, a_current_limited_supply_folds_back_when_the_motor_asks_for_more_than_it_can_give)
{
    model.Supply().Configure({ .currentLimitAmpere = 0.05f });
    model.Start();
    DriveHard(2000);

    EXPECT_LT(model.Supply().Voltage().Value(), supplyVolts * 0.9f);
}

TEST_F(TestPlantNonidealities, a_current_limit_above_the_demand_leaves_the_bus_alone)
{
    model.Supply().Configure({ .currentLimitAmpere = 1000.0f });
    model.Start();
    DriveHard(2000);

    EXPECT_NEAR(model.Supply().Voltage().Value(), supplyVolts, 1e-3f);
}

TEST_F(TestPlantNonidealities, stopping_releases_the_bus)
{
    model.Supply().Configure({ .sourceResistanceOhm = 1.0f });
    model.Start();
    DriveHard(2000);
    model.Stop();

    EXPECT_NEAR(model.Supply().Voltage().Value(), supplyVolts, 1e-6f);
}

TEST_F(TestPlantNonidealities, coulomb_friction_holds_a_rotor_that_the_torque_cannot_break_away)
{
    model.Mechanics().SetCoulombFriction(foc::NewtonMeter{ 10.0f });
    model.Start();
    DriveHard(2000);

    EXPECT_NEAR(model.Mechanics().Speed().Value(), 0.0f, 1e-6f);
}

TEST_F(TestPlantNonidealities, coulomb_friction_lowers_the_speed_the_same_drive_reaches)
{
    foc::ThreePhaseMotorModel frictionless{ foc::M_2310P_LN_04K::parameters, foc::Volts{ supplyVolts }, hal::Hertz{ 20000 } };
    frictionless.Start();
    model.Mechanics().SetCoulombFriction(foc::NewtonMeter{ 0.02f });
    model.Start();

    const foc::PhasePwmDutyCycles duty{ hal::DutyCycle::FromPercent(70), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(30) };
    for (int i = 0; i < 400; ++i)
    {
        frictionless.StepForTest(duty);
        model.StepForTest(duty);
    }

    EXPECT_LT(std::abs(model.Mechanics().Speed().Value()), std::abs(frictionless.Mechanics().Speed().Value()));
    EXPECT_GT(std::abs(frictionless.Mechanics().Speed().Value()), 0.1f);
}

TEST_F(TestPlantNonidealities, cogging_holds_the_rotor_in_a_detent_against_a_torque_below_its_amplitude)
{
    model.Mechanics().SetCogging(foc::NewtonMeter{ 0.004f }, 24);
    model.Mechanics().SetExternalTorque(foc::NewtonMeter{ -0.003f });
    model.Start();
    DriveCycles(20000, 50, 50, 50);

    const auto detent = 2.0f * std::numbers::pi_v<float> / 24.0f;
    EXPECT_GT(model.Mechanics().Angle().Value(), 0.0f);
    EXPECT_LT(model.Mechanics().Angle().Value(), detent / 4.0f);
}

TEST_F(TestPlantNonidealities, a_torque_beyond_the_cogging_amplitude_carries_the_rotor_past_its_detents)
{
    model.Mechanics().SetCogging(foc::NewtonMeter{ 0.004f }, 24);
    model.Mechanics().SetExternalTorque(foc::NewtonMeter{ -0.006f });
    model.Start();
    DriveCycles(20000, 50, 50, 50);

    EXPECT_GT(model.Mechanics().Speed().Value(), 0.1f);
}

TEST_F(TestPlantNonidealities, a_gripped_shaft_turns_at_the_hand_speed_whatever_the_drive_does)
{
    model.Start();
    model.Mechanics().Grip(foc::RadiansPerSecond{ -5.0f });
    DriveHard(2000);

    EXPECT_NEAR(model.Mechanics().Speed().Value(), -5.0f, 1e-6f);
    EXPECT_NEAR(model.Mechanics().Angle().Value(), -0.5f, 1e-3f);
}

TEST_F(TestPlantNonidealities, a_shaft_held_still_does_not_move_under_the_drive_torque)
{
    model.Start();
    model.Mechanics().Grip(foc::RadiansPerSecond{ 0.0f });
    DriveHard(2000);

    EXPECT_NEAR(model.Mechanics().Speed().Value(), 0.0f, 1e-6f);
    EXPECT_NEAR(model.Mechanics().Angle().Value(), 0.0f, 1e-6f);
}

TEST_F(TestPlantNonidealities, a_released_shaft_answers_the_drive_again)
{
    model.Start();
    model.Mechanics().Grip(foc::RadiansPerSecond{ -5.0f });
    DriveHard(2000);

    model.Mechanics().Grip(std::nullopt);
    DriveHard(200);

    EXPECT_GT(std::abs(model.Mechanics().Speed().Value() + 5.0f), 1.0f);
}

TEST_F(TestPlantNonidealities, a_quantised_encoder_reports_whole_counts)
{
    constexpr uint32_t counts{ 4000 };
    model.Encoder().SetResolution(counts);
    model.Start();
    DriveHard(300);

    const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(counts);
    const float reported = model.Encoder().Read().Value();
    EXPECT_NEAR(reported / step, std::round(reported / step), 1e-2f);
}

TEST_F(TestPlantNonidealities, a_quantised_encoder_stays_within_one_count_of_the_true_angle)
{
    constexpr uint32_t counts{ 4000 };
    model.Encoder().SetResolution(counts);
    model.Start();
    DriveHard(300);

    const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(counts);
    EXPECT_LE(std::abs(model.Mechanics().Angle().Value() - model.Encoder().Read().Value()), step * 1.01f);
}
