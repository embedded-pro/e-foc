#include "numerical/math/Tolerance.hpp"
#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/test_doubles/DriversMock.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    constexpr uint32_t baseFrequencyValue = 20000;
    const hal::Hertz baseFrequency{ baseFrequencyValue };
    const hal::Hertz lowPriorityFrequency{ 2000 };
    constexpr float tolerance = 1.0f;

    class TestFocPositionImpl
        : public ::testing::Test
    {
    public:
        void SetUp() override
        {
            EXPECT_CALL(lowPriorityInterruptMock, Register(_)).WillOnce(Invoke(&lowPriorityInterruptMock, &foc::LowPriorityInterruptMock::StoreHandler));

            focPosition.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterruptMock, lowPriorityFrequency);
            focPosition->SetPolePairs(polePairs);
            focPosition->SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });
            focPosition->SetSpeedTunings(foc::Volts{ 24.0f }, { 1.0f, 0.0f, 0.0f });
            focPosition->SetPositionTunings({ 1.0f, 0.0f, 0.0f });
        }

        foc::LowPriorityInterruptMock lowPriorityInterruptMock;
        std::optional<foc::FocPositionImpl> focPosition;
        std::size_t polePairs = 7;
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }
}

TEST_F(TestFocPositionImpl, zero_position_setpoint_produces_bounded_duty_cycles)
{
    focPosition->SetPoint(foc::Radians{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_GE(result.b.Value(), 0);
    EXPECT_LE(result.b.Value(), 100);
    EXPECT_GE(result.c.Value(), 0);
    EXPECT_LE(result.c.Value(), 100);
}

TEST_F(TestFocPositionImpl, duty_cycles_are_bounded_0_to_100)
{
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position{ 0.5f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_GE(result.b.Value(), 0);
    EXPECT_LE(result.b.Value(), 100);
    EXPECT_GE(result.c.Value(), 0);
    EXPECT_LE(result.c.Value(), 100);
}

TEST_F(TestFocPositionImpl, set_pole_pairs)
{
    focPosition->SetPolePairs(4);
    focPosition->SetPoint(foc::Radians{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocPositionImpl, enable_disable_cycle)
{
    focPosition->SetPoint(foc::Radians{ 0.5f });

    focPosition->Disable();
    focPosition->Enable();

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocPositionImpl, different_positions_produce_different_outputs)
{
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position1{ 0.0f };
    auto result1 = focPosition->Calculate(ZeroCurrents(), position1);

    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians position2{ 1.0f };
    auto result2 = focPosition->Calculate(ZeroCurrents(), position2);

    bool anyDifferent = (result1.a.Value() != result2.a.Value()) ||
                        (result1.b.Value() != result2.b.Value()) ||
                        (result1.c.Value() != result2.c.Value());

    EXPECT_TRUE(anyDifferent);
}

TEST_F(TestFocPositionImpl, position_pid_drives_speed_reference)
{
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position{ 0.0f };
    focPosition->Calculate(ZeroCurrents(), position);

    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians position2{ 0.01f };
    auto result = focPosition->Calculate(ZeroCurrents(), position2);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocPositionImpl, set_speed_tunings)
{
    focPosition->SetSpeedTunings(foc::Volts{ 24.0f }, { 2.0f, 0.1f, 0.0f });
    focPosition->SetPoint(foc::Radians{ 0.5f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocPositionImpl, set_position_tunings)
{
    focPosition->SetPositionTunings({ 5.0f, 0.5f, 0.0f });
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocPositionImpl, at_target_position_output_is_near_center)
{
    focPosition->SetPoint(foc::Radians{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(result.a.Value(), 50, tolerance);
    EXPECT_NEAR(result.b.Value(), 50, tolerance);
    EXPECT_NEAR(result.c.Value(), 50, tolerance);
}

TEST_F(TestFocPositionImpl, prescaler_triggers_low_priority_interrupt)
{
    focPosition->SetPoint(foc::Radians{ 0.5f });
    const uint32_t expectedPrescaler = baseFrequencyValue / lowPriorityFrequency.Value();

    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(1);

    for (uint32_t i = 0; i < expectedPrescaler; ++i)
    {
        foc::Radians position{ 0.0f };
        focPosition->Calculate(ZeroCurrents(), position);
    }
}
