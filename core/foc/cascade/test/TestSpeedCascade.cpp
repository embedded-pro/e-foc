#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "numerical/math/Tolerance.hpp"
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    constexpr uint32_t baseFrequencyValue = 20000;
    const hal::Hertz baseFrequency{ baseFrequencyValue };
    const hal::Hertz lowPriorityFrequency{ 2000 };
    constexpr float tolerance = 1.0f;

    foc::MotorModelParameters MotorParameters(std::size_t polePairs)
    {
        auto parameters = foc::MotorModelParameters{};
        parameters.resistance = foc::Ohm{ 1.0f };
        parameters.inductance = foc::MilliHenry{ 1.0f };
        parameters.fluxLinkage = foc::Weber{ 0.01f };
        parameters.busVoltage = foc::Volts{ 24.0f };
        parameters.samplingFrequency = baseFrequency;
        parameters.polePairs = polePairs;
        return parameters;
    }

    foc::MechanicalModelParameters MechanicalParameters()
    {
        auto parameters = foc::MechanicalModelParameters{};
        parameters.inertia = foc::NewtonMeterSecondSquared{ 0.001f };
        parameters.viscousFriction = foc::NewtonMeterSecondPerRadian{ 0.0001f };
        parameters.torqueConstant = foc::NewtonMeter{ 0.1f };
        parameters.maxCurrent = foc::Ampere{ 10.0f };
        parameters.samplingFrequency = lowPriorityFrequency;
        return parameters;
    }

    class TestSpeedCascade
        : public ::testing::Test
    {
    public:
        void SetUp() override
        {
            EXPECT_CALL(lowPriorityInterruptMock, Register(_)).WillOnce(Invoke(&lowPriorityInterruptMock, &foc::LowPriorityInterruptMock::StoreHandler));

            focSpeed.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterruptMock, lowPriorityFrequency);
            focSpeed->Configure(MotorParameters(polePairs));
            focSpeed->ConfigureMechanics(MechanicalParameters());
            focSpeed->SetCurrentTunings(foc::CurrentLoopTunings{});
            focSpeed->SetSpeedTunings(foc::SpeedLoopTunings{});
        }

        foc::LowPriorityInterruptMock lowPriorityInterruptMock;
        std::optional<foc::SpeedCascade> focSpeed;
        std::size_t polePairs = 7;
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }
}

TEST_F(TestSpeedCascade, zero_speed_setpoint_produces_bounded_duty_cycles)
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

TEST_F(TestSpeedCascade, duty_cycles_are_bounded_0_to_100)
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

TEST_F(TestSpeedCascade, set_pole_pairs)
{
    focSpeed->Configure(MotorParameters(4));
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestSpeedCascade, enable_disable_cycle)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 10.0f });

    focSpeed->Disable();
    focSpeed->Enable();

    foc::Radians position{ 0.0f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestSpeedCascade, different_positions_produce_different_outputs)
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

TEST_F(TestSpeedCascade, consecutive_calls_update_speed_estimation)
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

TEST_F(TestSpeedCascade, set_speed_tunings)
{
    focSpeed->SetSpeedTunings(foc::SpeedLoopTunings{});
    focSpeed->SetPoint(foc::RadiansPerSecond{ 5.0f });

    foc::Radians position{ 0.0f };
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestSpeedCascade, prescaler_triggers_low_priority_at_correct_rate)
{
    // baseFrequency = 20000, lowPriorityFrequency = 2000 → prescaler = 10
    // LowPriorityInterrupt::Trigger() must be called exactly once every 10 iterations.
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    const uint32_t prescaler = baseFrequencyValue / 2000;

    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(1);

    for (uint32_t i = 0; i < prescaler; ++i)
    {
        foc::Radians position{ 0.0f };
        focSpeed->Calculate(ZeroCurrents(), position);
    }
}

TEST_F(TestSpeedCascade, second_prescaler_cycle_triggers_again)
{
    focSpeed->SetPoint(foc::RadiansPerSecond{ 0.0f });

    const uint32_t prescaler = baseFrequencyValue / 2000;

    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(2);

    for (uint32_t i = 0; i < prescaler * 2; ++i)
    {
        foc::Radians position{ 0.0f };
        focSpeed->Calculate(ZeroCurrents(), position);
    }
}

TEST_F(TestSpeedCascade, the_cascade_supplies_the_current_envelope_and_rate_the_caller_cannot_know)
{
    // Callers outside the cascade have no view of maxCurrent or the outer loop rate, so they pass
    // zeros; the cascade must substitute its own before the speed loop validates the model.
    auto withoutLimits = MechanicalParameters();
    withoutLimits.maxCurrent = foc::Ampere{ 0.0f };
    withoutLimits.samplingFrequency = hal::Hertz{ 0 };

    focSpeed->ConfigureMechanics(withoutLimits);

    EXPECT_EQ(focSpeed->SelectSpeedAlgorithm(foc::SpeedAlgorithm::lqi), foc::SelectResult::ok);
    EXPECT_EQ(focSpeed->ActiveSpeedAlgorithm(), foc::SpeedAlgorithm::lqi);
}

TEST_F(TestSpeedCascade, a_speed_loop_configured_without_limits_still_drives_the_output)
{
    auto withoutLimits = MechanicalParameters();
    withoutLimits.maxCurrent = foc::Ampere{ 0.0f };
    withoutLimits.samplingFrequency = hal::Hertz{ 0 };

    focSpeed->ConfigureMechanics(withoutLimits);
    focSpeed->SetSpeedTunings(foc::SpeedLoopTunings{});
    focSpeed->Enable();
    focSpeed->SetPoint(foc::RadiansPerSecond{ 100.0f });

    foc::Radians position{ 0.0f };
    lowPriorityInterruptMock.TriggerHandler();
    auto result = focSpeed->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}
