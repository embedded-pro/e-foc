#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <optional>

namespace
{
    constexpr float currentTolerance{ 1e-6f };

    class TestFaultInjection
        : public ::testing::Test
    {
    protected:
        void DriveCycles(int cycles)
        {
            const foc::PhasePwmDutyCycles duty{
                hal::DutyCycle::FromPercent(60),
                hal::DutyCycle::FromPercent(50),
                hal::DutyCycle::FromPercent(40)
            };
            for (int i = 0; i < cycles; ++i)
                model.StepForTest(duty);
        }

        foc::ThreePhaseMotorModel model{
            foc::M_2310P_LN_04K::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 20000 }
        };
    };
}

TEST_F(TestFaultInjection, healthy_plant_draws_current_on_every_phase)
{
    model.Start();
    DriveCycles(50);

    const auto currents = model.CurrentSensor().Last();
    EXPECT_GT(std::abs(currents.a.Value()), currentTolerance);
    EXPECT_GT(std::abs(currents.b.Value()), currentTolerance);
    EXPECT_GT(std::abs(currents.c.Value()), currentTolerance);
}

TEST_F(TestFaultInjection, open_phase_carries_no_current)
{
    foc::InjectFaults(model, { .openPhaseA = true });
    model.Start();
    DriveCycles(50);

    const auto currents = model.CurrentSensor().Last();
    EXPECT_NEAR(currents.a.Value(), 0.0f, currentTolerance);
    EXPECT_GT(std::abs(currents.b.Value()), currentTolerance);
    EXPECT_GT(std::abs(currents.c.Value()), currentTolerance);
}

TEST_F(TestFaultInjection, a_fully_disconnected_motor_carries_no_current_at_all)
{
    foc::InjectFaults(model, { .openPhaseA = true, .openPhaseB = true, .openPhaseC = true });
    model.Start();
    DriveCycles(50);

    const auto currents = model.CurrentSensor().Last();
    EXPECT_NEAR(currents.a.Value(), 0.0f, currentTolerance);
    EXPECT_NEAR(currents.b.Value(), 0.0f, currentTolerance);
    EXPECT_NEAR(currents.c.Value(), 0.0f, currentTolerance);
}

TEST_F(TestFaultInjection, a_disconnected_motor_does_not_turn)
{
    foc::InjectFaults(model, { .openPhaseA = true, .openPhaseB = true, .openPhaseC = true });
    model.Start();
    DriveCycles(200);

    EXPECT_NEAR(model.Encoder().Read().Value(), 0.0f, 1e-4f);
}

TEST_F(TestFaultInjection, stuck_encoder_freezes_the_reported_angle_while_the_rotor_turns)
{
    model.Start();
    DriveCycles(200);
    const auto angleBeforeFault = model.Encoder().Read().Value();

    foc::InjectFaults(model, { .encoderStuck = true });
    DriveCycles(400);

    EXPECT_NEAR(model.Encoder().Read().Value(), angleBeforeFault, 1e-4f);
}

TEST_F(TestFaultInjection, supply_voltage_scale_reduces_the_effective_bus_voltage)
{
    foc::InjectFaults(model, { .supplyVoltageScale = 0.5f });

    EXPECT_NEAR(model.Supply().Voltage().Value(), 12.0f, 1e-6f);
}

TEST_F(TestFaultInjection, a_sagging_supply_produces_less_current_than_a_healthy_one)
{
    model.Start();
    DriveCycles(50);
    const auto healthy = std::abs(model.CurrentSensor().Last().a.Value());

    foc::ThreePhaseMotorModel sagging{
        foc::M_2310P_LN_04K::parameters,
        foc::Volts{ 24.0f },
        hal::Hertz{ 20000 }
    };
    foc::InjectFaults(sagging, { .supplyVoltageScale = 0.25f });
    sagging.Start();
    const foc::PhasePwmDutyCycles duty{ hal::DutyCycle::FromPercent(60), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(40) };
    for (int i = 0; i < 50; ++i)
        sagging.StepForTest(duty);

    EXPECT_LT(std::abs(sagging.CurrentSensor().Last().a.Value()), healthy);
}

TEST_F(TestFaultInjection, the_same_seed_reproduces_the_same_noise_sequence)
{
    const foc::ThreePhaseMotorModel::NoiseConfig noise{ .sigmaAmpere = 0.5f };
    const foc::PhasePwmDutyCycles duty{ hal::DutyCycle::FromPercent(60), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(40) };

    auto run = [&noise, &duty](uint32_t seed)
    {
        foc::ThreePhaseMotorModel model{
            foc::M_2310P_LN_04K::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 20000 }
        };
        model.CurrentSensor().SetNoise(noise);
        foc::SeedNoise(model, seed);
        model.Start();
        for (int i = 0; i < 100; ++i)
            model.StepForTest(duty);
        return model.CurrentSensor().Last().a.Value();
    };

    EXPECT_EQ(run(1234u), run(1234u));
    EXPECT_NE(run(1234u), run(4321u));
}
