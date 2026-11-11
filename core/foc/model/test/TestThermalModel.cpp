#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestThermalModel
        : public ::testing::Test
    {
    protected:
        foc::ThreePhaseMotorModel model{
            foc::M_2310P_LN_04K::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 100000 }
        };
    };
}

TEST_F(TestThermalModel, effective_resistance_scales_with_temperature)
{
    constexpr float testTemp = 125.0f;
    model.Thermal().SetTemperature(testTemp);

    const float rBase = foc::M_2310P_LN_04K::parameters.R.Value();
    const float expectedR = rBase * (1.0f + 0.00393f * (testTemp - 25.0f));
    const float actualR = foc::Ohm{ model.Winding().resistance }.Value();

    EXPECT_NEAR(actualR, expectedR, expectedR * 0.005f);
}

TEST_F(TestThermalModel, a_hot_magnet_carries_less_flux_linkage)
{
    foc::ThermalConfig config{};
    config.magnetFluxTempCoeff = 0.0011f;
    model.Thermal().Configure(config);
    model.Thermal().SetTemperature(125.0f);

    const float psiBase = foc::M_2310P_LN_04K::parameters.psi_f.Value();

    EXPECT_NEAR(model.Winding().fluxLinkage, psiBase * (1.0f - 0.0011f * 100.0f), psiBase * 1e-5f);
}

TEST_F(TestThermalModel, a_magnet_at_the_reference_temperature_keeps_its_flux_linkage)
{
    foc::ThermalConfig config{};
    config.magnetFluxTempCoeff = 0.0011f;
    model.Thermal().Configure(config);
    model.Thermal().SetTemperature(config.referenceCelsius);

    EXPECT_NEAR(model.Winding().fluxLinkage, foc::M_2310P_LN_04K::parameters.psi_f.Value(), 1e-9f);
}

TEST_F(TestThermalModel, without_a_magnet_coefficient_the_flux_linkage_does_not_drift)
{
    model.Thermal().SetTemperature(125.0f);

    EXPECT_NEAR(model.Winding().fluxLinkage, foc::M_2310P_LN_04K::parameters.psi_f.Value(), 1e-9f);
}

TEST_F(TestThermalModel, temperature_rises_when_current_flows)
{
    constexpr int cycles = 10000;
    const foc::PhasePwmDutyCycles duty{
        hal::DutyCycle::FromPercent(75),
        hal::DutyCycle::FromPercent(50),
        hal::DutyCycle::FromPercent(25)
    };
    for (int i = 0; i < cycles; ++i)
        model.StepForTest(duty);

    EXPECT_GT(model.Thermal().Temperature(), 25.0f);
}

TEST_F(TestThermalModel, reset_temperature_restores_ambient)
{
    model.Thermal().SetTemperature(80.0f);
    model.Thermal().Reset();

    EXPECT_NEAR(model.Thermal().Temperature(), 25.0f, 1e-4f);
}
