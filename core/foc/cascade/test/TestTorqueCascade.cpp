#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/DutyConversion.hpp"
#include <cmath>
#include <gmock/gmock.h>

namespace
{
    float DutyPercent(hal::DutyCycle duty)
    {
        return 100.0f * foc::DutyFraction(duty);
    }

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
            focTorque.emplace(testMaxCurrent);
            focTorque->Configure(MotorParameters(polePairs));
            focTorque->Enable();
        }

        const foc::Ampere testMaxCurrent{ 10.0f };

        std::optional<foc::TorqueCascade> focTorque;
        const std::size_t polePairs{ 7 };
    };

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }

    void ExpectValidDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_LE(DutyPercent(duty.a), 100);
        EXPECT_LE(DutyPercent(duty.b), 100);
        EXPECT_LE(DutyPercent(duty.c), 100);
    }

    void ExpectCentredDuty(const foc::PhasePwmDutyCycles& duty)
    {
        EXPECT_NEAR(DutyPercent(duty.a), 50, tolerance);
        EXPECT_NEAR(DutyPercent(duty.b), 50, tolerance);
        EXPECT_NEAR(DutyPercent(duty.c), 50, tolerance);
    }

    bool IsOffCentre(const foc::PhasePwmDutyCycles& duty)
    {
        return std::abs(DutyPercent(duty.a) - 50) > tolerance ||
               std::abs(DutyPercent(duty.b) - 50) > tolerance ||
               std::abs(DutyPercent(duty.c) - 50) > tolerance;
    }
}

TEST_F(TestTorqueCascade, zero_setpoint_and_zero_currents_gives_50_percent_duty_cycles)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(DutyPercent(result.a), 50, tolerance);
    EXPECT_NEAR(DutyPercent(result.b), 50, tolerance);
    EXPECT_NEAR(DutyPercent(result.c), 50, tolerance);
}

TEST_F(TestTorqueCascade, duty_cycles_are_bounded_0_to_100)
{
    focTorque->SetPoint({ foc::Ampere{ 10.0f }, foc::Ampere{ 10.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.5f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    ExpectValidDuty(result);
}

// theta_e = theta_m * pole_pairs: p pole pairs at a mechanical angle must land on the same
// electrical angle - and therefore the same duty cycles - as 1 pole pair at p times that angle.
TEST_F(TestTorqueCascade, electrical_angle_scales_with_pole_pairs)
{
    const foc::PhaseCurrents currents{ foc::Ampere{ 0.4f }, foc::Ampere{ -0.1f }, foc::Ampere{ -0.3f } };
    const float mechanicalAngle = 0.3f;

    for (std::size_t poles : { std::size_t{ 1 }, std::size_t{ 2 }, std::size_t{ 4 }, std::size_t{ 7 } })
    {
        foc::TorqueCascade scaled{ testMaxCurrent };
        scaled.Configure(MotorParameters(poles));
        scaled.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } });
        scaled.Enable();

        foc::TorqueCascade reference{ testMaxCurrent };
        reference.Configure(MotorParameters(1));
        reference.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } });
        reference.Enable();

        foc::Radians scaledPosition{ mechanicalAngle };
        foc::Radians referencePosition{ mechanicalAngle * static_cast<float>(poles) };

        auto scaledResult = scaled.Calculate(currents, scaledPosition);
        auto referenceResult = reference.Calculate(currents, referencePosition);

        EXPECT_EQ(DutyPercent(scaledResult.a), DutyPercent(referenceResult.a)) << "pole pairs " << poles;
        EXPECT_EQ(DutyPercent(scaledResult.b), DutyPercent(referenceResult.b)) << "pole pairs " << poles;
        EXPECT_EQ(DutyPercent(scaledResult.c), DutyPercent(referenceResult.c)) << "pole pairs " << poles;
    }
}

TEST_F(TestTorqueCascade, enable_disable_cycle)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::PhasePwmDutyCycles wound{ hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0) };
    for (int sample = 0; sample != 50; ++sample)
    {
        foc::Radians position{ 0.2f };
        wound = focTorque->Calculate(ZeroCurrents(), position);
    }

    ASSERT_TRUE(IsOffCentre(wound));

    focTorque->Disable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    focTorque->Enable();

    foc::Radians position{ 0.2f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    ExpectCentredDuty(result);
}

TEST_F(TestTorqueCascade, different_positions_produce_different_outputs)
{
    focTorque->SetPoint({ foc::Ampere{ 1.0f }, foc::Ampere{ 0.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position1{ 0.0f };
    auto result1 = focTorque->Calculate(ZeroCurrents(), position1);

    foc::Radians position2{ 1.0f };
    auto result2 = focTorque->Calculate(ZeroCurrents(), position2);

    bool anyDifferent = (DutyPercent(result1.a) != DutyPercent(result2.a)) ||
                        (DutyPercent(result1.b) != DutyPercent(result2.b)) ||
                        (DutyPercent(result1.c) != DutyPercent(result2.c));

    EXPECT_TRUE(anyDifferent);
}

TEST_F(TestTorqueCascade, non_zero_current_setpoint_produces_non_centered_output)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 5.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    bool anyNon50 = (DutyPercent(result.a) != 50) || (DutyPercent(result.b) != 50) || (DutyPercent(result.c) != 50);
    EXPECT_TRUE(anyNon50);
}

TEST_F(TestTorqueCascade, pid_is_the_default_current_algorithm)
{
    EXPECT_EQ(focTorque->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(TestTorqueCascade, algorithm_selection_is_rejected_while_enabled)
{
    EXPECT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::busy);
    EXPECT_EQ(focTorque->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(TestTorqueCascade, algorithm_selection_is_accepted_while_disabled)
{
    focTorque->Disable();

    EXPECT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
    EXPECT_EQ(focTorque->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::deadbeat);
}

TEST_F(TestTorqueCascade, selected_algorithm_drives_the_output)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 5.0f } });

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    ExpectValidDuty(result);
    bool anyNon50 = (DutyPercent(result.a) != 50) || (DutyPercent(result.b) != 50) || (DutyPercent(result.c) != 50);
    EXPECT_TRUE(anyNon50);
}

TEST_F(TestTorqueCascade, a_stationary_rotor_produces_no_back_emf_feedforward)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    foc::Radians position{ 0.0f };
    for (int sample = 0; sample != 100; ++sample)
        focTorque->Calculate(ZeroCurrents(), position);

    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(DutyPercent(result.a), 50, tolerance);
    EXPECT_NEAR(DutyPercent(result.b), 50, tolerance);
    EXPECT_NEAR(DutyPercent(result.c), 50, tolerance);
}

TEST_F(TestTorqueCascade, a_spinning_rotor_drives_the_back_emf_feedforward)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    // Torque mode has no outer loop, so the cascade must derive speed from the angle it is handed
    constexpr float mechanicalStepPerSample = 0.01f;
    float angle = 0.0f;
    foc::PhasePwmDutyCycles result{ hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0) };

    for (int sample = 0; sample != 200; ++sample)
    {
        foc::Radians position{ angle };
        result = focTorque->Calculate(ZeroCurrents(), position);
        angle = foc::detail::PositionWithWrapAround(angle + mechanicalStepPerSample);
    }

    const bool anyOffCentre = IsOffCentre(result);

    EXPECT_TRUE(anyOffCentre);
}

TEST_F(TestTorqueCascade, the_speed_estimate_does_not_spike_on_the_first_sample_after_enable)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });

    // A rotor parked far from zero must not read as a huge step on the first sample
    foc::Radians position{ 3.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(DutyPercent(result.a), 50, tolerance);
    EXPECT_NEAR(DutyPercent(result.b), 50, tolerance);
    EXPECT_NEAR(DutyPercent(result.c), 50, tolerance);
}

TEST_F(TestTorqueCascade, the_sliding_mode_algorithm_drives_the_inverter)
{
    focTorque->Disable();
    ASSERT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::slidingMode), foc::SelectResult::ok);
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 2.0f } });

    foc::Radians position{ 0.0f };
    auto result = focTorque->Calculate(ZeroCurrents(), position);

    ExpectValidDuty(result);
    EXPECT_TRUE(DutyPercent(result.a) != 50 || DutyPercent(result.b) != 50 || DutyPercent(result.c) != 50);
}

TEST_F(TestTorqueCascade, a_setpoint_inside_the_envelope_is_taken_unchanged)
{
    focTorque->SetPoint({ foc::Ampere{ 3.0f }, foc::Ampere{ 4.0f } });

    foc::Radians position{ 0.0f };
    const auto inside = focTorque->Calculate(
        foc::PhaseCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } }, position);

    focTorque->SetPoint({ foc::Ampere{ 3.0f }, foc::Ampere{ 4.0f } });
    position = foc::Radians{ 0.0f };
    const auto again = focTorque->Calculate(
        foc::PhaseCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } }, position);

    EXPECT_EQ(DutyPercent(inside.a), DutyPercent(again.a));
}

TEST_F(TestTorqueCascade, a_setpoint_outside_the_envelope_is_scaled_onto_it)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1000.0f } });

    foc::Radians huge{ 0.0f };
    const auto clamped = focTorque->Calculate(
        foc::PhaseCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } }, huge);

    focTorque->SetPoint({ foc::Ampere{ 0.0f }, testMaxCurrent });

    foc::Radians atLimit{ 0.0f };
    const auto expected = focTorque->Calculate(
        foc::PhaseCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } }, atLimit);

    EXPECT_NEAR(DutyPercent(expected.a), DutyPercent(clamped.a), 1e-3f);
    EXPECT_NEAR(DutyPercent(expected.b), DutyPercent(clamped.b), 1e-3f);
    EXPECT_NEAR(DutyPercent(expected.c), DutyPercent(clamped.c), 1e-3f);
}

TEST_F(TestTorqueCascade, the_envelope_bounds_the_current_vector_not_each_axis)
{
    focTorque->SetPoint({ testMaxCurrent, testMaxCurrent });

    foc::Radians diagonal{ 0.0f };
    const auto clamped = focTorque->Calculate(
        foc::PhaseCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } }, diagonal);

    const auto onCircle = testMaxCurrent.Value() / std::sqrt(2.0f);
    focTorque->SetPoint({ foc::Ampere{ onCircle }, foc::Ampere{ onCircle } });

    foc::Radians reference{ 0.0f };
    const auto expected = focTorque->Calculate(
        foc::PhaseCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } }, reference);

    EXPECT_NEAR(DutyPercent(expected.a), DutyPercent(clamped.a), 1e-3f);
    EXPECT_NEAR(DutyPercent(expected.b), DutyPercent(clamped.b), 1e-3f);
    EXPECT_NEAR(DutyPercent(expected.c), DutyPercent(clamped.c), 1e-3f);
}

TEST_F(TestTorqueCascade, the_observed_motion_carries_the_measured_speed_and_demands_none)
{
    constexpr float step{ 0.0005f };

    for (int tick = 0; tick != 400; ++tick)
    {
        foc::Radians position{ static_cast<float>(tick) * step };
        focTorque->Calculate(ZeroCurrents(), position);
    }

    const auto observation = focTorque->ObserveMotion();
    EXPECT_NEAR(observation.measuredSpeed.Value(), step * 20000.0f, 0.05f * step * 20000.0f);
    EXPECT_NEAR(observation.demandedSpeed.Value(), 0.0f, 1e-6f);
    EXPECT_NEAR(observation.positionError.Value(), 0.0f, 1e-6f);
}

TEST_F(TestTorqueCascade, a_disabled_cascade_observes_no_motion)
{
    constexpr float step{ 0.0005f };

    focTorque->Enable();
    for (int tick = 0; tick != 400; ++tick)
    {
        foc::Radians position{ static_cast<float>(tick) * step };
        focTorque->Calculate(ZeroCurrents(), position);
    }

    focTorque->Disable();

    const auto observation = focTorque->ObserveMotion();
    EXPECT_NEAR(observation.measuredSpeed.Value(), 0.0f, 1e-6f);
    EXPECT_NEAR(observation.measuredTorqueCurrent.Value(), 0.0f, 1e-6f);
}
