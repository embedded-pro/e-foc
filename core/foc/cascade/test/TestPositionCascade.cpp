#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
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

    constexpr float observableError{ 0.05f };

    foc::CurrentLoopTunings UnsaturatedCurrentTunings()
    {
        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = 628.3185f;
        return tunings;
    }

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
            EXPECT_CALL(lowPriorityInterruptMock, Unregister()).WillOnce(Invoke(&lowPriorityInterruptMock, &foc::LowPriorityInterruptMock::ClearHandler));

            focPosition.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterruptMock, lowPriorityFrequency);
            focPosition->Configure(MotorParameters(polePairs));
            focPosition->ConfigureMechanics(MechanicalParameters());
            focPosition->SetCurrentTunings(foc::CurrentLoopTunings{});
            focPosition->SetSpeedTunings(foc::SpeedLoopTunings{});
            focPosition->SetPositionTunings(foc::PositionLoopTunings{});
        }

        testing::StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        std::optional<foc::PositionCascade> focPosition;
        const std::size_t polePairs{ 7 };
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }

    const foc::PhasePwmDutyCycles centredDuty{ hal::Percent{ 50 }, hal::Percent{ 50 }, hal::Percent{ 50 } };

    void ExpectValidDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_LE(duty.a.Value(), 100);
        EXPECT_LE(duty.b.Value(), 100);
        EXPECT_LE(duty.c.Value(), 100);
    }

    void ExpectCentredDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_NEAR(duty.a.Value(), 50, tolerance);
        EXPECT_NEAR(duty.b.Value(), 50, tolerance);
        EXPECT_NEAR(duty.c.Value(), 50, tolerance);
    }

    void ExpectSameDuty(const foc::PhasePwmDutyCycles& duty, const foc::PhasePwmDutyCycles& expected)
    {
        EXPECT_EQ(duty.a.Value(), expected.a.Value());
        EXPECT_EQ(duty.b.Value(), expected.b.Value());
        EXPECT_EQ(duty.c.Value(), expected.c.Value());
    }

    bool DutiesDiffer(const foc::PhasePwmDutyCycles& left, const foc::PhasePwmDutyCycles& right)
    {
        return left.a.Value() != right.a.Value() ||
               left.b.Value() != right.b.Value() ||
               left.c.Value() != right.c.Value();
    }

    void ExpectOffCentreDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_TRUE(DutiesDiffer(duty, centredDuty));
    }

    struct PositionCascadeUnderTest
    {
        PositionCascadeUnderTest(std::size_t polePairs, const foc::SpeedLoopTunings& speedTunings, const foc::PositionLoopTunings& positionTunings, foc::PositionAlgorithm algorithm)
        {
            EXPECT_CALL(lowPriorityInterrupt, Register(_)).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::StoreHandler));
            EXPECT_CALL(lowPriorityInterrupt, Unregister()).WillOnce(Invoke(&lowPriorityInterrupt, &foc::LowPriorityInterruptMock::ClearHandler));

            cascade.emplace(foc::Ampere{ 10.0f }, baseFrequency, lowPriorityInterrupt, lowPriorityFrequency);
            cascade->Configure(MotorParameters(polePairs));
            cascade->ConfigureMechanics(MechanicalParameters());
            cascade->SetCurrentTunings(UnsaturatedCurrentTunings());
            cascade->SetSpeedTunings(speedTunings);
            EXPECT_EQ(cascade->SelectPositionAlgorithm(algorithm), foc::SelectResult::ok);
            EXPECT_EQ(cascade->SetPositionTunings(positionTunings), foc::SelectResult::ok);
        }

        testing::StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterrupt;
        std::optional<foc::PositionCascade> cascade;
    };

    foc::PhasePwmDutyCycles DutyAfterOuterCycles(PositionCascadeUnderTest& positionCascade, float setPoint, float measured, int outerCycles = 1)
    {
        positionCascade.cascade->Enable();
        positionCascade.cascade->SetPoint(foc::Radians{ setPoint });

        for (int cycle = 0; cycle != outerCycles; ++cycle)
        {
            foc::Radians position{ measured };
            positionCascade.cascade->Calculate(ZeroCurrents(), position);
            positionCascade.lowPriorityInterrupt.TriggerHandler();
        }

        foc::Radians position{ measured };
        return positionCascade.cascade->Calculate(ZeroCurrents(), position);
    }
}

TEST_F(TestPositionCascade, zero_position_setpoint_produces_bounded_duty_cycles)
{
    focPosition->SetPoint(foc::Radians{ 0.0f });

    foc::Radians position{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    ExpectValidDuty(result);
}

TEST_F(TestPositionCascade, duty_cycles_are_bounded_0_to_100)
{
    focPosition->SetPoint(foc::Radians{ 1.0f });

    foc::Radians position{ 0.5f };
    auto result = focPosition->Calculate(ZeroCurrents(), position);

    ExpectValidDuty(result);
}

TEST_F(TestPositionCascade, set_pole_pairs)
{
    constexpr float mechanicalAngle{ 0.3f };

    for (std::size_t poles : { std::size_t{ 2 }, std::size_t{ 4 }, std::size_t{ 7 } })
    {
        SCOPED_TRACE(poles);

        PositionCascadeUnderTest scaled{ poles, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };
        PositionCascadeUnderTest reference{ 1, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };

        scaled.cascade->Enable();
        reference.cascade->Enable();
        scaled.cascade->SetPoint(foc::Radians{ 0.002f });
        reference.cascade->SetPoint(foc::Radians{ 0.002f });

        foc::Radians scaledStart{ 0.0f };
        foc::Radians referenceStart{ 0.0f };
        scaled.cascade->Calculate(ZeroCurrents(), scaledStart);
        reference.cascade->Calculate(ZeroCurrents(), referenceStart);
        scaled.lowPriorityInterrupt.TriggerHandler();
        reference.lowPriorityInterrupt.TriggerHandler();

        foc::Radians scaledPosition{ mechanicalAngle };
        foc::Radians referencePosition{ mechanicalAngle * static_cast<float>(poles) };

        ExpectSameDuty(scaled.cascade->Calculate(ZeroCurrents(), scaledPosition), reference.cascade->Calculate(ZeroCurrents(), referencePosition));
    }
}

TEST_F(TestPositionCascade, enable_disable_cycle)
{
    PositionCascadeUnderTest positionCascade{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };

    positionCascade.cascade->Enable();
    positionCascade.cascade->SetPoint(foc::Radians{ observableError });

    foc::Radians position{ 0.0f };
    positionCascade.cascade->Calculate(ZeroCurrents(), position);
    for (int cycle = 0; cycle != 20; ++cycle)
        positionCascade.lowPriorityInterrupt.TriggerHandler();

    foc::Radians woundPosition{ 0.0f };
    auto wound = positionCascade.cascade->Calculate(ZeroCurrents(), woundPosition);

    positionCascade.cascade->Disable();
    positionCascade.cascade->Enable();

    PositionCascadeUnderTest fresh{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };
    fresh.cascade->Enable();
    fresh.cascade->SetPoint(foc::Radians{ observableError });

    foc::Radians restarted{ 0.0f };
    foc::Radians freshPosition{ 0.0f };
    auto afterEnable = positionCascade.cascade->Calculate(ZeroCurrents(), restarted);
    auto firstOfFresh = fresh.cascade->Calculate(ZeroCurrents(), freshPosition);

    EXPECT_TRUE(DutiesDiffer(wound, afterEnable));
    ExpectSameDuty(afterEnable, firstOfFresh);
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
    constexpr float positionError{ observableError };

    PositionCascadeUnderTest positionCascade{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };

    testing::StrictMock<foc::LowPriorityInterruptMock> speedInterrupt;
    EXPECT_CALL(speedInterrupt, Register(_)).WillOnce(Invoke(&speedInterrupt, &foc::LowPriorityInterruptMock::StoreHandler));
    EXPECT_CALL(speedInterrupt, Unregister()).WillOnce(Invoke(&speedInterrupt, &foc::LowPriorityInterruptMock::ClearHandler));

    foc::SpeedCascade speedCascade{ foc::Ampere{ 10.0f }, baseFrequency, speedInterrupt, lowPriorityFrequency };
    speedCascade.Configure(MotorParameters(polePairs));
    speedCascade.ConfigureMechanics(MechanicalParameters());
    speedCascade.SetCurrentTunings(UnsaturatedCurrentTunings());
    speedCascade.SetSpeedTunings(foc::SpeedLoopTunings{});
    speedCascade.Enable();
    speedCascade.SetPoint(foc::RadiansPerSecond{ positionError * foc::PositionLoopTunings{}.bandwidth });

    foc::Radians start{ 0.0f };
    speedCascade.Calculate(ZeroCurrents(), start);
    speedInterrupt.TriggerHandler();

    foc::Radians secondSample{ 0.0f };
    auto equivalentSpeedCommand = speedCascade.Calculate(ZeroCurrents(), secondSample);

    ExpectOffCentreDuty(equivalentSpeedCommand);
    ExpectSameDuty(DutyAfterOuterCycles(positionCascade, positionError, 0.0f), equivalentSpeedCommand);
}

TEST_F(TestPositionCascade, set_speed_tunings)
{
    auto slowTunings = foc::SpeedLoopTunings{};
    slowTunings.bandwidth = foc::SpeedLoopTunings{}.bandwidth / 10.0f;

    PositionCascadeUnderTest slowLoop{ polePairs, slowTunings, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };
    PositionCascadeUnderTest fastLoop{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };
    PositionCascadeUnderTest atTarget{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };

    auto slowResult = DutyAfterOuterCycles(slowLoop, 0.005f, 0.0f);
    auto fastResult = DutyAfterOuterCycles(fastLoop, 0.005f, 0.0f);

    EXPECT_TRUE(DutiesDiffer(slowResult, fastResult));
    ExpectCentredDuty(DutyAfterOuterCycles(atTarget, 0.0f, 0.0f));
}

TEST_F(TestPositionCascade, set_position_tunings)
{
    auto slowTunings = foc::PositionLoopTunings{};
    slowTunings.bandwidth = foc::PositionLoopTunings{}.bandwidth / 10.0f;

    PositionCascadeUnderTest slowLoop{ polePairs, foc::SpeedLoopTunings{}, slowTunings, foc::PositionAlgorithm::pid };
    PositionCascadeUnderTest fastLoop{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };
    PositionCascadeUnderTest atTarget{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };

    auto slowResult = DutyAfterOuterCycles(slowLoop, 0.005f, 0.0f);
    auto fastResult = DutyAfterOuterCycles(fastLoop, 0.005f, 0.0f);

    EXPECT_TRUE(DutiesDiffer(slowResult, fastResult));
    ExpectCentredDuty(DutyAfterOuterCycles(atTarget, 0.0f, 0.0f));
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
    PositionCascadeUnderTest stateFeedback{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::lqr };
    PositionCascadeUnderTest defaultAlgorithm{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };

    auto result = DutyAfterOuterCycles(stateFeedback, 0.002f, 0.0f);

    ExpectOffCentreDuty(result);
    EXPECT_TRUE(DutiesDiffer(result, DutyAfterOuterCycles(defaultAlgorithm, 0.002f, 0.0f)));
}

TEST_F(TestPositionCascade, the_cascade_p_algorithm_drives_the_inverter)
{
    auto tunings = foc::PositionLoopTunings{};
    tunings.integralWeight = 100.0f;

    PositionCascadeUnderTest proportional{ polePairs, foc::SpeedLoopTunings{}, tunings, foc::PositionAlgorithm::cascadeP };
    PositionCascadeUnderTest defaultAlgorithm{ polePairs, foc::SpeedLoopTunings{}, tunings, foc::PositionAlgorithm::pid };

    auto result = DutyAfterOuterCycles(proportional, observableError, 0.0f);

    ExpectOffCentreDuty(result);
    EXPECT_TRUE(DutiesDiffer(result, DutyAfterOuterCycles(defaultAlgorithm, observableError, 0.0f)));
}

TEST_F(TestPositionCascade, the_two_dof_algorithm_drives_the_inverter)
{
    constexpr float shapedSetPoint{ 1.0f };
    constexpr int outerCycles{ 3 };

    PositionCascadeUnderTest twoDof{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::twoDof };
    PositionCascadeUnderTest defaultAlgorithm{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };

    auto result = DutyAfterOuterCycles(twoDof, shapedSetPoint, 0.0f, outerCycles);

    ExpectOffCentreDuty(result);
    EXPECT_TRUE(DutiesDiffer(result, DutyAfterOuterCycles(defaultAlgorithm, shapedSetPoint, 0.0f, outerCycles)));
}

TEST_F(TestPositionCascade, a_non_positive_reference_time_constant_leaves_the_two_dof_prefilter_transparent)
{
    auto transparentTunings = foc::PositionLoopTunings{};
    transparentTunings.referenceTimeConstant = 0.0f;

    PositionCascadeUnderTest transparent{ polePairs, foc::SpeedLoopTunings{}, transparentTunings, foc::PositionAlgorithm::twoDof };
    PositionCascadeUnderTest shaped{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::twoDof };
    PositionCascadeUnderTest feedbackOnly{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };

    auto withoutPrefilter = DutyAfterOuterCycles(feedbackOnly, observableError, 0.0f);

    ExpectOffCentreDuty(withoutPrefilter);
    ExpectSameDuty(DutyAfterOuterCycles(transparent, observableError, 0.0f), withoutPrefilter);
    EXPECT_TRUE(DutiesDiffer(DutyAfterOuterCycles(shaped, observableError, 0.0f), withoutPrefilter));
}

TEST_F(TestPositionCascade, a_setpoint_across_the_encoder_seam_moves_the_short_way)
{
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float shortWay{ 0.5f };
    const float measured{ pi - 0.001f };

    constexpr int outerCycles{ 2 };

    PositionCascadeUnderTest acrossTheSeam{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::lqr };
    PositionCascadeUnderTest forward{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::lqr };
    PositionCascadeUnderTest backward{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::lqr };

    auto seamResult = DutyAfterOuterCycles(acrossTheSeam, -pi + (shortWay - 0.001f), measured, outerCycles);
    auto forwardResult = DutyAfterOuterCycles(forward, measured + shortWay, measured, outerCycles);
    auto backwardResult = DutyAfterOuterCycles(backward, measured - shortWay, measured, outerCycles);

    ExpectSameDuty(seamResult, forwardResult);
    EXPECT_TRUE(DutiesDiffer(seamResult, backwardResult));
}

TEST_F(TestPositionCascade, a_current_commanding_algorithm_bypasses_the_speed_loop)
{
    auto slowTunings = foc::SpeedLoopTunings{};
    slowTunings.bandwidth = foc::SpeedLoopTunings{}.bandwidth / 100.0f;

    PositionCascadeUnderTest currentCommanding{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::lqr };
    PositionCascadeUnderTest currentCommandingSlowSpeedLoop{ polePairs, slowTunings, foc::PositionLoopTunings{}, foc::PositionAlgorithm::lqr };
    PositionCascadeUnderTest speedCommanding{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::cascadeP };
    PositionCascadeUnderTest speedCommandingSlowSpeedLoop{ polePairs, slowTunings, foc::PositionLoopTunings{}, foc::PositionAlgorithm::cascadeP };

    auto currentCommandingResult = DutyAfterOuterCycles(currentCommanding, observableError, 0.0f);

    ExpectOffCentreDuty(currentCommandingResult);
    ExpectOffCentreDuty(DutyAfterOuterCycles(speedCommanding, observableError, 0.0f));
    ExpectSameDuty(DutyAfterOuterCycles(currentCommandingSlowSpeedLoop, observableError, 0.0f), currentCommandingResult);
    EXPECT_TRUE(DutiesDiffer(DutyAfterOuterCycles(speedCommandingSlowSpeedLoop, observableError, 0.0f), DutyAfterOuterCycles(speedCommanding, observableError, 0.0f)));
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

TEST_F(TestPositionCascade, the_position_loop_receives_the_effective_current_envelope_and_rate)
{
    PositionCascadeUnderTest cascade{ polePairs, foc::SpeedLoopTunings{}, foc::PositionLoopTunings{}, foc::PositionAlgorithm::pid };

    auto placeholders = MechanicalParameters();
    placeholders.maxCurrent = foc::Ampere{ 0.0f };
    placeholders.samplingFrequency = hal::Hertz{ 0 };
    cascade.cascade->ConfigureMechanics(placeholders);

    EXPECT_EQ(cascade.cascade->SelectPositionAlgorithm(foc::PositionAlgorithm::lqr), foc::SelectResult::ok);
    ExpectOffCentreDuty(DutyAfterOuterCycles(cascade, observableError, 0.0f));
}

TEST_F(TestPositionCascade, speed_command_frequency_equals_outer_loop_frequency)
{
    EXPECT_EQ(focPosition->SpeedCommandFrequency().Value(), lowPriorityFrequency.Value());
}

TEST_F(TestPositionCascade, enable_speed_command_allows_direct_speed_reference_bypass)
{
    focPosition->EnableSpeedCommand();
    focPosition->CommandSpeed(foc::RadiansPerSecond{ 50.0f });

    foc::Radians position{ 0.0f };
    focPosition->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians secondPosition{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), secondPosition);

    ExpectValidDuty(result);
}

TEST_F(TestPositionCascade, disable_speed_command_restores_normal_position_control)
{
    focPosition->EnableSpeedCommand();
    focPosition->CommandSpeed(foc::RadiansPerSecond{ 50.0f });

    foc::Radians runPosition{ 0.0f };
    focPosition->Calculate(ZeroCurrents(), runPosition);
    lowPriorityInterruptMock.TriggerHandler();

    focPosition->DisableSpeedCommand();

    focPosition->Enable();
    focPosition->SetPoint(foc::Radians{ 0.0f });

    foc::Radians centredPosition{ 0.0f };
    auto result = focPosition->Calculate(ZeroCurrents(), centredPosition);

    ExpectValidDuty(result);
}

TEST_F(TestPositionCascade, speed_command_produces_non_centred_duty_at_nonzero_speed)
{
    focPosition->EnableSpeedCommand();
    focPosition->CommandSpeed(foc::RadiansPerSecond{ 50.0f });

    foc::Radians position{ 0.0f };
    focPosition->Calculate(ZeroCurrents(), position);
    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians second{ 0.0f };
    auto withSpeed = focPosition->Calculate(ZeroCurrents(), second);

    focPosition->DisableSpeedCommand();
    focPosition->Enable();
    focPosition->SetPoint(foc::Radians{ 0.0f });

    foc::Radians third{ 0.0f };
    focPosition->Calculate(ZeroCurrents(), third);
    lowPriorityInterruptMock.TriggerHandler();

    foc::Radians fourth{ 0.0f };
    auto atTarget = focPosition->Calculate(ZeroCurrents(), fourth);

    EXPECT_TRUE(DutiesDiffer(withSpeed, atTarget));
}
