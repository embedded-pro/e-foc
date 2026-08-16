#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/foc/interfaces/test_doubles/OnlineEstimatorsMock.hpp"
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

    class TestPositionCascade
        : public ::testing::Test
    {
    public:
        void SetUp() override
        {
            EXPECT_CALL(lowPriorityInterruptMock, Register(_)).WillOnce(Invoke(&lowPriorityInterruptMock, &foc::LowPriorityInterruptMock::StoreHandler));

            focPosition.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterruptMock, lowPriorityFrequency);
            focPosition->Configure(MotorParameters(polePairs));
            focPosition->ConfigureMechanics(MechanicalParameters());
            focPosition->SetCurrentTunings(foc::CurrentLoopTunings{});
            focPosition->SetSpeedTunings(foc::SpeedLoopTunings{});
            focPosition->SetPositionTunings(foc::PositionLoopTunings{});
        }

        testing::StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        std::optional<foc::PositionCascade> focPosition;
        std::size_t polePairs = 7;
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }
}

TEST_F(TestPositionCascade, zero_position_setpoint_produces_bounded_duty_cycles)
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

TEST_F(TestPositionCascade, duty_cycles_are_bounded_0_to_100)
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

TEST_F(TestPositionCascade, set_pole_pairs)
{
    focPosition->Configure(MotorParameters(4));
    focPosition->SetPoint(foc::Radians{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestPositionCascade, enable_disable_cycle)
{
    focPosition->SetPoint(foc::Radians{ 0.5f });

    focPosition->Disable();
    focPosition->Enable();

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestPositionCascade, different_positions_produce_different_outputs)
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

TEST_F(TestPositionCascade, position_pid_drives_speed_reference)
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

TEST_F(TestPositionCascade, set_speed_tunings)
{
    focPosition->SetSpeedTunings(foc::SpeedLoopTunings{});
    focPosition->SetPoint(foc::Radians{ 0.5f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestPositionCascade, set_position_tunings)
{
    focPosition->SetPositionTunings(foc::PositionLoopTunings{});
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestPositionCascade, at_target_position_output_is_near_center)
{
    focPosition->SetPoint(foc::Radians{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(result.a.Value(), 50, tolerance);
    EXPECT_NEAR(result.b.Value(), 50, tolerance);
    EXPECT_NEAR(result.c.Value(), 50, tolerance);
}

TEST_F(TestPositionCascade, prescaler_triggers_low_priority_interrupt)
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

TEST_F(TestPositionCascade, pid_is_the_default_position_algorithm)
{
    EXPECT_EQ(focPosition->ActivePositionAlgorithm(), foc::PositionAlgorithm::pid);
}

TEST_F(TestPositionCascade, every_position_algorithm_is_selectable_once_the_mechanics_are_known)
{
    for (auto algorithm : { foc::PositionAlgorithm::cascadeP,
             foc::PositionAlgorithm::lqr,
             foc::PositionAlgorithm::lqi,
             foc::PositionAlgorithm::twoDof,
             foc::PositionAlgorithm::pid })
    {
        EXPECT_EQ(focPosition->SelectPositionAlgorithm(algorithm), foc::SelectResult::ok);
        EXPECT_EQ(focPosition->ActivePositionAlgorithm(), algorithm);
    }
}

TEST_F(TestPositionCascade, selecting_a_position_algorithm_is_refused_while_enabled)
{
    focPosition->Enable();

    EXPECT_EQ(focPosition->SelectPositionAlgorithm(foc::PositionAlgorithm::lqr), foc::SelectResult::busy);
    EXPECT_EQ(focPosition->ActivePositionAlgorithm(), foc::PositionAlgorithm::pid);
}

TEST_F(TestPositionCascade, retuning_the_position_loop_is_refused_while_enabled)
{
    focPosition->Enable();

    EXPECT_EQ(focPosition->SetPositionTunings(foc::PositionLoopTunings{}), foc::SelectResult::busy);
}

TEST_F(TestPositionCascade, retuning_is_accepted_again_once_disabled)
{
    focPosition->Enable();
    focPosition->Disable();

    EXPECT_EQ(focPosition->SetPositionTunings(foc::PositionLoopTunings{}), foc::SelectResult::ok);
}

TEST_F(TestPositionCascade, an_unknown_position_algorithm_is_rejected)
{
    EXPECT_EQ(focPosition->SelectPositionAlgorithm(static_cast<foc::PositionAlgorithm>(200)), foc::SelectResult::invalidAlgorithm);
}

TEST_F(TestPositionCascade, a_current_commanding_algorithm_still_drives_the_inverter)
{
    ASSERT_EQ(focPosition->SelectPositionAlgorithm(foc::PositionAlgorithm::lqr), foc::SelectResult::ok);
    focPosition->Enable();
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestPositionCascade, the_cascade_p_algorithm_drives_the_inverter)
{
    ASSERT_EQ(focPosition->SelectPositionAlgorithm(foc::PositionAlgorithm::cascadeP), foc::SelectResult::ok);
    focPosition->Enable();
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position{ 0.0f };
    focPosition->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestPositionCascade, the_two_dof_algorithm_drives_the_inverter)
{
    ASSERT_EQ(focPosition->SelectPositionAlgorithm(foc::PositionAlgorithm::twoDof), foc::SelectResult::ok);
    focPosition->Enable();
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position{ 0.0f };
    for (int sample = 0; sample != 5; ++sample)
    {
        focPosition->Calculate(ZeroCurrents(), position);
        lowPriorityInterruptMock.TriggerHandler();
    }

    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestPositionCascade, a_non_positive_reference_time_constant_leaves_the_two_dof_prefilter_transparent)
{
    auto tunings = foc::PositionLoopTunings{};
    tunings.referenceTimeConstant = 0.0f;

    ASSERT_EQ(focPosition->SelectPositionAlgorithm(foc::PositionAlgorithm::twoDof), foc::SelectResult::ok);
    ASSERT_EQ(focPosition->SetPositionTunings(tunings), foc::SelectResult::ok);
    focPosition->Enable();
    focPosition->SetPoint(foc::Radians{ 0.5f });

    foc::Radians position{ 0.0f };
    focPosition->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestPositionCascade, a_setpoint_across_the_encoder_seam_moves_the_short_way)
{
    constexpr float pi = std::numbers::pi_v<float>;

    ASSERT_EQ(focPosition->SelectPositionAlgorithm(foc::PositionAlgorithm::cascadeP), foc::SelectResult::ok);
    focPosition->Enable();

    // Both directions across the wrap, so the error folds each way rather than running the long way round
    for (auto [setPoint, measured] : { std::pair{ -pi + 0.1f, pi - 0.1f }, std::pair{ pi - 0.1f, -pi + 0.1f } })
    {
        focPosition->SetPoint(foc::Radians{ setPoint });

        foc::Radians position{ measured };
        focPosition->Calculate(ZeroCurrents(), position);
        lowPriorityInterruptMock.TriggerHandler();
        auto result = focPosition->Calculate(ZeroCurrents(), position);

        EXPECT_GE(result.a.Value(), 0);
        EXPECT_LE(result.a.Value(), 100);
    }
}

TEST_F(TestPositionCascade, a_current_commanding_algorithm_bypasses_the_speed_loop)
{
    ASSERT_EQ(focPosition->SelectPositionAlgorithm(foc::PositionAlgorithm::lqr), foc::SelectResult::ok);
    focPosition->Enable();
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position{ 0.0f };
    focPosition->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    EXPECT_GE(result.a.Value(), 0);
    EXPECT_LE(result.a.Value(), 100);
}

TEST_F(TestPositionCascade, the_inner_and_speed_algorithms_are_reported)
{
    EXPECT_EQ(focPosition->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
    EXPECT_EQ(focPosition->ActiveSpeedAlgorithm(), foc::SpeedAlgorithm::pid);
}

TEST_F(TestPositionCascade, registered_online_estimators_are_fed_from_the_outer_loop)
{
    testing::StrictMock<foc::OnlineMechanicalEstimatorMock> mechanicalEstimator;
    testing::StrictMock<foc::OnlineElectricalEstimatorMock> electricalEstimator;

    focPosition->SetOnlineMechanicalEstimator(mechanicalEstimator);
    focPosition->SetOnlineElectricalEstimator(electricalEstimator);
    focPosition->Enable();
    focPosition->SetPoint(foc::Radians{ 0.5f });

    EXPECT_CALL(mechanicalEstimator, Update(_, _, _));
    EXPECT_CALL(electricalEstimator, Update(_, _, _, _));

    foc::Radians position{ 0.0f };
    focPosition->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();
}
