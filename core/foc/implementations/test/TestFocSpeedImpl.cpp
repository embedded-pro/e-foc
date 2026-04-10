#include "numerical/math/Tolerance.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
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

    class TestFocSpeedImpl
        : public ::testing::Test
    {
    public:
        void SetUp() override
        {
            EXPECT_CALL(lowPriorityInterruptMock, Register(_)).WillOnce(Invoke(&lowPriorityInterruptMock, &foc::LowPriorityInterruptMock::StoreHandler));

            focSpeed.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterruptMock, lowPriorityFrequency);
            focSpeed->SetPolePairs(polePairs);
            focSpeed->SetCurrentTunings(foc::Volts{ 24.0f }, { { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });
            focSpeed->SetSpeedTunings(foc::Volts{ 24.0f }, { 1.0f, 0.0f, 0.0f });
        }

        foc::LowPriorityInterruptMock lowPriorityInterruptMock;
        std::optional<foc::FocSpeedImpl> focSpeed;
        std::size_t polePairs = 7;
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }
}

TEST_F(TestFocSpeedImpl, zero_speed_setpoint_produces_bounded_duty_cycles)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_GE(result.b.Value(), 0);
    EXPECT_LE(result.b.Value(), 100);
    EXPECT_GE(result.c.Value(), 0);
    EXPECT_LE(result.c.Value(), 100);
}

TEST_F(TestFocSpeedImpl, duty_cycles_are_bounded_0_to_100)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 100.0f });

    foc::Radians position{ 0.5f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
    EXPECT_GE(result.b.Value(), 0);
    EXPECT_LE(result.b.Value(), 100);
    EXPECT_GE(result.c.Value(), 0);
    EXPECT_LE(result.c.Value(), 100);
}

TEST_F(TestFocSpeedImpl, set_pole_pairs)
{
    focSpeed->SetPolePairs(4);
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocSpeedImpl, enable_disable_cycle)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 10.0f });

    focSpeed->Disable();
    focSpeed->Enable();

    foc::Radians position{ 0.0f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestFocSpeedImpl, different_positions_produce_different_outputs)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 10.0f });

    foc::Radians position1{ 0.0f };
    auto result1 = focSpeed->Calculate(ZeroCurrents(), position1);

    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians position2{ 1.0f };
    auto result2 = focSpeed->Calculate(ZeroCurrents(), position2);

    bool anyDifferent = (result1.a.Value() != result2.a.Value()) ||
                        (result1.b.Value() != result2.b.Value()) ||
                        (result1.c.Value() != result2.c.Value());

    EXPECT_TRUE(anyDifferent);
}

TEST_F(TestFocSpeedImpl, consecutive_calls_update_speed_estimation)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 10.0f });

    foc::Radians position1{ 0.0f };
    auto result1 = focSpeed->Calculate(ZeroCurrents(), position1);

    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians position2{ 0.01f };
    auto result2 = focSpeed->Calculate(ZeroCurrents(), position2);

    EXPECT_GE(result2.a.Value(), 0);
    EXPECT_LE(result2.a.Value(), 100);
}

TEST_F(TestFocSpeedImpl, set_speed_tunings)
{
    focSpeed->SetSpeedTunings(foc::Volts{ 24.0f }, { 2.0f, 0.1f, 0.0f });
    focSpeed->SetPoint(foc::RadiansPerSecond{ 5.0f });

    foc::Radians position{ 0.0f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}
