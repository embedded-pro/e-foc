#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include "tools/virtual_motor/platform/PlantSignals.hpp"
#include "gtest/gtest.h"

namespace
{
    using virtual_motor::Signal;

    const foc::PhasePwmDutyCycles driving{ hal::DutyCycle::FromPercent(60), hal::DutyCycle::FromPercent(45), hal::DutyCycle::FromPercent(40) };

    float At(const std::array<float, virtual_motor::port::signalCount>& sample, Signal signal)
    {
        return sample[static_cast<std::size_t>(signal)];
    }

    class TestPlantSignals
        : public testing::Test
    {
    public:
        TestPlantSignals()
        {
            model.Start();
            for (int period = 0; period != 200; ++period)
                model.StepForTest(driving);
        }

        foc::ThreePhaseMotorModel model{ foc::M_2310P_LN_04K::parameters, foc::Volts{ 24.0f }, hal::Hertz{ 20000 } };
        virtual_motor::PlantSignals signals{ model, foc::M_2310P_LN_04K::parameters.p };
    };
}

TEST_F(TestPlantSignals, there_is_one_signal_per_data_model_row)
{
    EXPECT_EQ(static_cast<std::size_t>(Signal::windingTemperature) + 1, virtual_motor::port::signalCount);
}

TEST_F(TestPlantSignals, carries_the_true_and_the_measured_phase_currents_and_the_applied_duties)
{
    model.StepForTest(driving);
    const auto sample = signals.Sample(driving, foc::NewtonMeter{ 0.0f });

    EXPECT_FLOAT_EQ(At(sample, Signal::currentA), model.Electrical().Currents().a.Value());
    EXPECT_FLOAT_EQ(At(sample, Signal::currentC), model.Electrical().Currents().c.Value());
    EXPECT_FLOAT_EQ(At(sample, Signal::measuredCurrentB), model.CurrentSensor().Last().b.Value());
    EXPECT_NEAR(At(sample, Signal::dutyA), 0.60f, 1e-4f);
    EXPECT_NEAR(At(sample, Signal::dutyC), 0.40f, 1e-4f);
}

TEST_F(TestPlantSignals, the_stator_voltage_is_the_clarke_transform_of_the_leg_voltages)
{
    model.StepForTest(driving);
    const auto sample = signals.Sample(driving, foc::NewtonMeter{ 0.0f });

    foc::Clarke clarke;
    const auto alphaBeta = clarke.Forward(foc::ThreePhase{ At(sample, Signal::legVoltageA), At(sample, Signal::legVoltageB), At(sample, Signal::legVoltageC) });
    EXPECT_NEAR(At(sample, Signal::statorVoltageAlpha), alphaBeta.alpha, 1e-4f);
    EXPECT_NEAR(At(sample, Signal::statorVoltageBeta), alphaBeta.beta, 1e-4f);
    EXPECT_GT(At(sample, Signal::legVoltageA), At(sample, Signal::legVoltageC));
}

TEST_F(TestPlantSignals, the_electrical_torque_follows_the_true_dq_currents)
{
    model.StepForTest(driving);
    const auto sample = signals.Sample(driving, foc::NewtonMeter{ 0.0f });

    const auto& motor = foc::M_2310P_LN_04K::parameters;
    const auto dq = model.Electrical().LastDqCurrents();
    const auto expected = 1.5f * motor.p * (motor.psi_f.Value() * dq.q + (motor.Ld.Value() - motor.Lq.Value()) * dq.d * dq.q);
    EXPECT_FLOAT_EQ(At(sample, Signal::currentD), dq.d);
    EXPECT_FLOAT_EQ(At(sample, Signal::currentQ), dq.q);
    EXPECT_NEAR(At(sample, Signal::electricalTorque), expected, 1e-6f);
}

TEST_F(TestPlantSignals, carries_the_mechanics_the_torques_and_the_supply)
{
    model.Mechanics().SetExternalTorque(foc::NewtonMeter{ -0.02f });
    model.StepForTest(driving);
    const auto sample = signals.Sample(driving, foc::NewtonMeter{ 0.03f });

    EXPECT_FLOAT_EQ(At(sample, Signal::rotorAngle), model.Mechanics().Angle().Value());
    EXPECT_FLOAT_EQ(At(sample, Signal::encoderAngle), model.Encoder().Read().Value());
    EXPECT_FLOAT_EQ(At(sample, Signal::rotorSpeed), model.Mechanics().Speed().Value());
    EXPECT_FLOAT_EQ(At(sample, Signal::loadTorque), 0.03f);
    EXPECT_FLOAT_EQ(At(sample, Signal::externalTorque), -0.02f);
    EXPECT_FLOAT_EQ(At(sample, Signal::busVoltage), model.Supply().Voltage().Value());
    EXPECT_FLOAT_EQ(At(sample, Signal::busCurrent), model.Supply().Current().Value());
    EXPECT_FLOAT_EQ(At(sample, Signal::windingTemperature), model.Thermal().Temperature());
}
