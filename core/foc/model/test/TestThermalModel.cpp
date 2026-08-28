#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "motor_parameters/Jk42bls01X038ed.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestThermalModel
        : public ::testing::Test
    {
    protected:
        infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

        foc::ThreePhaseMotorModel model{
            foc::JK42BLS01_X038ED::parameters,
            foc::Volts{ 24.0f },
            hal::Hertz{ 100000 },
            std::optional<std::size_t>{}
        };
    };
}

TEST_F(TestThermalModel, effective_resistance_scales_with_temperature)
{
    constexpr float testTemp = 125.0f;
    model.SetWindingTemperatureForTest(testTemp);

    const float rBase = foc::JK42BLS01_X038ED::parameters.R.Value();
    const float expectedR = rBase * (1.0f + 0.00393f * (testTemp - 25.0f));
    const float actualR = model.EffectiveResistance().Value();

    EXPECT_NEAR(actualR, expectedR, expectedR * 0.005f);
}

TEST_F(TestThermalModel, temperature_rises_when_current_flows)
{
    constexpr int cycles = 10000;
    const foc::PhasePwmDutyCycles duty{
        hal::Percent{ 75 },
        hal::Percent{ 50 },
        hal::Percent{ 25 }
    };
    for (int i = 0; i < cycles; ++i)
        model.StepForTest(duty);

    EXPECT_GT(model.WindingTemperatureCelsius(), 25.0f);
}

TEST_F(TestThermalModel, reset_temperature_restores_ambient)
{
    model.SetWindingTemperatureForTest(80.0f);
    model.ResetTemperature();

    EXPECT_FLOAT_EQ(model.WindingTemperatureCelsius(), 25.0f);
}
