#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
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
                hal::Percent{ 60 },
                hal::Percent{ 50 },
                hal::Percent{ 40 }
            };
            for (int i = 0; i < cycles; ++i)
                model.StepForTest(duty);
        }

        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        foc::ThreePhaseMotorModel model{
            foc::M_2310P_LN_04K::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 20000 },
            std::optional<std::size_t>{},
            false
        };
    };
}

TEST_F(TestFaultInjection, healthy_plant_draws_current_on_every_phase)
{
    model.Start();
    DriveCycles(50);

    const auto currents = model.LastMeasuredCurrents();
    EXPECT_GT(std::abs(currents.a.Value()), currentTolerance);
    EXPECT_GT(std::abs(currents.b.Value()), currentTolerance);
    EXPECT_GT(std::abs(currents.c.Value()), currentTolerance);
}

TEST_F(TestFaultInjection, open_phase_carries_no_current)
{
    model.SetFaultInjection({ .openPhaseA = true });
    model.Start();
    DriveCycles(50);

    const auto currents = model.LastMeasuredCurrents();
    EXPECT_NEAR(currents.a.Value(), 0.0f, currentTolerance);
    EXPECT_GT(std::abs(currents.b.Value()), currentTolerance);
    EXPECT_GT(std::abs(currents.c.Value()), currentTolerance);
}

TEST_F(TestFaultInjection, a_fully_disconnected_motor_carries_no_current_at_all)
{
    model.SetFaultInjection({ .openPhaseA = true, .openPhaseB = true, .openPhaseC = true });
    model.Start();
    DriveCycles(50);

    const auto currents = model.LastMeasuredCurrents();
    EXPECT_NEAR(currents.a.Value(), 0.0f, currentTolerance);
    EXPECT_NEAR(currents.b.Value(), 0.0f, currentTolerance);
    EXPECT_NEAR(currents.c.Value(), 0.0f, currentTolerance);
}

TEST_F(TestFaultInjection, a_disconnected_motor_does_not_turn)
{
    model.SetFaultInjection({ .openPhaseA = true, .openPhaseB = true, .openPhaseC = true });
    model.Start();
    DriveCycles(200);

    EXPECT_NEAR(model.Read().Value(), 0.0f, 1e-4f);
}

TEST_F(TestFaultInjection, stuck_encoder_freezes_the_reported_angle_while_the_rotor_turns)
{
    model.Start();
    DriveCycles(200);
    const auto angleBeforeFault = model.Read().Value();

    model.SetFaultInjection({ .encoderStuck = true });
    DriveCycles(400);

    EXPECT_NEAR(model.Read().Value(), angleBeforeFault, 1e-4f);
}

TEST_F(TestFaultInjection, supply_voltage_scale_reduces_the_effective_bus_voltage)
{
    model.SetFaultInjection({ .supplyVoltageScale = 0.5f });

    EXPECT_NEAR(model.EffectiveSupplyVoltage().Value(), 12.0f, 1e-6f);
}

TEST_F(TestFaultInjection, a_sagging_supply_produces_less_current_than_a_healthy_one)
{
    model.Start();
    DriveCycles(50);
    const auto healthy = std::abs(model.LastMeasuredCurrents().a.Value());

    foc::ThreePhaseMotorModel sagging{
        foc::M_2310P_LN_04K::parameters,
        foc::Volts{ 24.0f },
        hal::Hertz{ 20000 },
        std::optional<std::size_t>{},
        false
    };
    sagging.SetFaultInjection({ .supplyVoltageScale = 0.25f });
    sagging.Start();
    const foc::PhasePwmDutyCycles duty{ hal::Percent{ 60 }, hal::Percent{ 50 }, hal::Percent{ 40 } };
    for (int i = 0; i < 50; ++i)
        sagging.StepForTest(duty);

    EXPECT_LT(std::abs(sagging.LastMeasuredCurrents().a.Value()), healthy);
}

TEST_F(TestFaultInjection, the_same_seed_reproduces_the_same_noise_sequence)
{
    const foc::ThreePhaseMotorModel::NoiseConfig noise{ .sigmaAmpere = 0.5f };
    const foc::PhasePwmDutyCycles duty{ hal::Percent{ 60 }, hal::Percent{ 50 }, hal::Percent{ 40 } };

    auto run = [&noise, &duty](uint32_t seed)
    {
        foc::ThreePhaseMotorModel model{
            foc::M_2310P_LN_04K::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 20000 },
            std::optional<std::size_t>{},
            false
        };
        model.SetAdcNoise(noise);
        model.SetRandomSeed(seed);
        model.Start();
        for (int i = 0; i < 100; ++i)
            model.StepForTest(duty);
        return model.LastMeasuredCurrents().a.Value();
    };

    EXPECT_EQ(run(1234u), run(1234u));
    EXPECT_NE(run(1234u), run(4321u));
}
