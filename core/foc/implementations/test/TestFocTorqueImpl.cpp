#include "numerical/math/Tolerance.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    constexpr float tolerance = 1.0f;

    class TestFocTorqueImpl
        : public ::testing::Test
    {
    public:
        void SetUp() override
        {
            focTorque.emplace();
            focTorque->SetPolePairs(polePairs);
            focTorque->Enable();
        }

        std::optional<foc::FocTorqueImpl> focTorque;
        std::size_t polePairs = 7;
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }
}

TEST_F(TestFocTorqueImpl, zero_setpoint_and_zero_currents_gives_50_percent_duty_cycles)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(result.a.Value(), 50, tolerance);
    EXPECT_NEAR(result.b.Value(), 50, tolerance);
    EXPECT_NEAR(result.c.Value(), 50, tolerance);
}

TEST_F(TestFocTorqueImpl, duty_cycles_are_bounded_0_to_100)
{
    focTorque->SetPoint({ foc::Ampere{ 10.0f }, foc::Ampere{ 10.0f } });
    focTorque->SetCurrentTunings(foc::Volts{ 24.0f }, { { 10.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f } });

    foc::Radians position{ 0.5f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_GE(result.b.Value(), 0);
    EXPECT_LE(result.b.Value(), 100);
    EXPECT_GE(result.c.Value(), 0);
    EXPECT_LE(result.c.Value(), 100);
}

TEST_F(TestFocTorqueImpl, set_pole_pairs)
{
    focTorque->SetPolePairs(4);
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocTorqueImpl, enable_disable_cycle)
{
    focTorque->SetPoint({ foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });

    focTorque->Disable();
    focTorque->Enable();

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocTorqueImpl, different_positions_produce_different_outputs)
{
    focTorque->SetPoint({ foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });

    foc::Radians position1{ 0.0f };
    auto result1 = focTorque->Calculate(ZeroCurrents(), position1);

    foc::Radians position2{ 1.0f };
    auto result2 = focTorque->Calculate(ZeroCurrents(), position2);

    bool anyDifferent = (result1.a.Value() != result2.a.Value()) ||
                        (result1.b.Value() != result2.b.Value()) ||
                        (result1.c.Value() != result2.c.Value());

    EXPECT_TRUE(anyDifferent);
}

TEST_F(TestFocTorqueImpl, non_zero_current_setpoint_produces_non_centered_output)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 5.0f } });
    focTorque->SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    bool anyNon50 = (result.a.Value() != 50) || (result.b.Value() != 50) || (result.c.Value() != 50);
    EXPECT_TRUE(anyNon50);
}
