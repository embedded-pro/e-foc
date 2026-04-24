#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "tools/simulator/model/Jk42bls01X038ed.hpp"
#include "tools/simulator/model/Model.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestThermalModel
        : public ::testing::Test
    {
    protected:
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        simulator::ThreePhaseMotorModel model{
            simulator::JK42BLS01_X038ED::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 100000 },
            std::optional<std::size_t>{}
        };
    };
}

TEST_F(TestThermalModel, effective_resistance_scales_with_temperature)
{
    // R(T) = R0 * (1 + alpha * (T - T_ambient))
    // With T_ambient = 25°C, T = 125°C, alpha = 0.00393:
    // R(125) = R0 * (1 + 0.00393 * 100) = R0 * 1.393
    constexpr float testTemp = 125.0f;
    model.SetWindingTemperatureForTest(testTemp);

    const float rBase = simulator::JK42BLS01_X038ED::parameters.R.Value();
    const float expectedR = rBase * (1.0f + 0.00393f * (testTemp - 25.0f));
    const float actualR = model.EffectiveResistance().Value();

    EXPECT_NEAR(actualR, expectedR, expectedR * 0.005f)
        << "R(125°C) should be ~1.393 * R0";
}

TEST_F(TestThermalModel, temperature_rises_when_current_flows)
{
    // Drive model directly with large duty asymmetry to push current
    constexpr int cycles = 10000; // 100 ms at 100 kHz
    const foc::PhasePwmDutyCycles duty{
        hal::Percent{ 75 },
        hal::Percent{ 50 },
        hal::Percent{ 25 }
    };
    for (int i = 0; i < cycles; ++i)
        model.StepForTest(duty);

    const float ambientCelsius = 25.0f;
    EXPECT_GT(model.WindingTemperatureCelsius(), ambientCelsius)
        << "Winding temperature should rise above ambient when current flows";
}

TEST_F(TestThermalModel, reset_temperature_restores_ambient)
{
    model.SetWindingTemperatureForTest(80.0f);
    model.ResetTemperature();

    EXPECT_FLOAT_EQ(model.WindingTemperatureCelsius(), 25.0f);
}
