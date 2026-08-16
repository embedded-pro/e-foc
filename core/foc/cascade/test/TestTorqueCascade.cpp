#include "core/foc/cascade/TorqueCascade.hpp"
#include "numerical/math/Tolerance.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float tolerance = 1.0f;

    foc::MotorModelParameters MotorParameters(std::size_t polePairs)
    {
        auto parameters = foc::MotorModelParameters{};
        parameters.resistance = foc::Ohm{ 1.0f };
        parameters.inductance = foc::MilliHenry{ 1.0f };
        parameters.fluxLinkage = foc::Weber{ 0.01f };
        parameters.busVoltage = foc::Volts{ 24.0f };
        parameters.samplingFrequency = hal::Hertz{ 20000 };
        parameters.polePairs = polePairs;
        return parameters;
    }

    class TestTorqueCascade
        : public ::testing::Test
    {
    public:
        void SetUp() override
        {
            focTorque.emplace();
            focTorque->Configure(MotorParameters(polePairs));
            focTorque->Enable();
        }

        std::optional<foc::TorqueCascade> focTorque;
        std::size_t polePairs = 7;
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }
}

TEST_F(TestTorqueCascade, zero_setpoint_and_zero_currents_gives_50_percent_duty_cycles)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(result.a.Value(), 50, tolerance);
    EXPECT_NEAR(result.b.Value(), 50, tolerance);
    EXPECT_NEAR(result.c.Value(), 50, tolerance);
}

TEST_F(TestTorqueCascade, duty_cycles_are_bounded_0_to_100)
{
    focTorque->SetPoint({ foc::Ampere{ 10.0f }, foc::Ampere{ 10.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.5f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_GE(result.b.Value(), 0);
    EXPECT_LE(result.b.Value(), 100);
    EXPECT_GE(result.c.Value(), 0);
    EXPECT_LE(result.c.Value(), 100);
}

TEST_F(TestTorqueCascade, set_pole_pairs)
{
    focTorque->Configure(MotorParameters(4));
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestTorqueCascade, enable_disable_cycle)
{
    focTorque->SetPoint({ foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    focTorque->Disable();
    focTorque->Enable();

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestTorqueCascade, different_positions_produce_different_outputs)
{
    focTorque->SetPoint({ foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position1{ 0.0f };
    auto result1 = focTorque->Calculate(ZeroCurrents(), position1);

    foc::Radians position2{ 1.0f };
    auto result2 = focTorque->Calculate(ZeroCurrents(), position2);

    bool anyDifferent = (result1.a.Value() != result2.a.Value()) ||
                        (result1.b.Value() != result2.b.Value()) ||
                        (result1.c.Value() != result2.c.Value());

    EXPECT_TRUE(anyDifferent);
}

TEST_F(TestTorqueCascade, non_zero_current_setpoint_produces_non_centered_output)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 5.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    bool anyNon50 = (result.a.Value() != 50) || (result.b.Value() != 50) || (result.c.Value() != 50);
    EXPECT_TRUE(anyNon50);
}
