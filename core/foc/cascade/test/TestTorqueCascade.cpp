#include "core/foc/cascade/CurrentStream.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "numerical/math/AngleUtilities.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>
#include <type_traits>

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

    foc::PhaseCurrents ZeroCurrents()
    {
        return { foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } };
    }

    foc::PhasePwmDutyCycles DutyAfterConstantSpeed(uint32_t dutyUpdateDelayPeriods, std::size_t polePairs, float mechanicalStep, float angleOffset)
    {
        auto parameters = MotorParameters(polePairs);
        parameters.dutyUpdateDelayPeriods = dutyUpdateDelayPeriods;

        foc::TorqueCascade cascade{ foc::Ampere{ 10.0f } };
        cascade.Configure(parameters);
        cascade.SetCurrentTunings(foc::CurrentLoopTunings{});
        cascade.SetPoint({ foc::Ampere{ 0.5f }, foc::Ampere{ 1.0f } });
        cascade.Enable();

        foc::PhasePwmDutyCycles duty{};
        for (int sample = 0; sample != 400; ++sample)
        {
            foc::Radians position{ angleOffset + static_cast<float>(sample) * mechanicalStep };
            duty = cascade.Calculate(ZeroCurrents(), position);
        }
        return duty;
    }

    void ExpectSameDuty(const foc::PhasePwmDutyCycles& actual, const foc::PhasePwmDutyCycles& expected)
    {
        EXPECT_NEAR(DutyPercent(actual.a), DutyPercent(expected.a), 0.05f);
        EXPECT_NEAR(DutyPercent(actual.b), DutyPercent(expected.b), 0.05f);
        EXPECT_NEAR(DutyPercent(actual.c), DutyPercent(expected.c), 0.05f);
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

    constexpr float mechanicalStepPerSample = 0.01f;
    float angle = 0.0f;
    foc::PhasePwmDutyCycles result{ hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0) };

    for (int sample = 0; sample != 200; ++sample)
    {
        foc::Radians position{ angle };
        result = focTorque->Calculate(ZeroCurrents(), position);
        angle = math::WrapToPi(angle + mechanicalStepPerSample);
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

TEST_F(TestTorqueCascade, a_current_command_drives_the_q_axis_current_it_asks_for)
{
    focTorque->EnableCurrentCommand();
    focTorque->CommandCurrent(foc::Ampere{ 2.0f });
    foc::Radians position{ 0.0f };
    const auto commanded = focTorque->Calculate(ZeroCurrents(), position);

    foc::TorqueCascade reference{ testMaxCurrent };
    reference.Configure(MotorParameters(polePairs));
    reference.Enable();
    reference.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 2.0f } });
    foc::Radians referencePosition{ 0.0f };
    const auto expected = reference.Calculate(ZeroCurrents(), referencePosition);

    EXPECT_NEAR(DutyPercent(commanded.a), DutyPercent(expected.a), 1e-3f);
    EXPECT_NEAR(DutyPercent(commanded.b), DutyPercent(expected.b), 1e-3f);
    EXPECT_NEAR(DutyPercent(commanded.c), DutyPercent(expected.c), 1e-3f);
}

TEST_F(TestTorqueCascade, a_current_command_starts_from_zero_whatever_setpoint_was_held)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 5.0f } });
    focTorque->Disable();

    focTorque->EnableCurrentCommand();
    foc::Radians position{ 0.0f };

    ExpectCentredDuty(focTorque->Calculate(ZeroCurrents(), position));
}

TEST_F(TestTorqueCascade, a_current_command_reports_the_mean_q_axis_voltage_it_applies)
{
    focTorque->EnableCurrentCommand();
    focTorque->CommandCurrent(foc::Ampere{ 2.0f });

    for (int tick = 0; tick != 100; ++tick)
    {
        foc::Radians position{ 0.0f };
        focTorque->Calculate(ZeroCurrents(), position);
    }

    EXPECT_GT(focTorque->QuadratureVoltage().Value(), 0.0f);

    focTorque->DisableCurrentCommand();
    EXPECT_NEAR(focTorque->QuadratureVoltage().Value(), 0.0f, 1e-6f);
}

TEST_F(TestTorqueCascade, outside_a_current_command_no_q_axis_voltage_is_kept)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 2.0f } });

    for (int tick = 0; tick != 100; ++tick)
    {
        foc::Radians position{ 0.0f };
        focTorque->Calculate(ZeroCurrents(), position);
    }

    EXPECT_NEAR(focTorque->QuadratureVoltage().Value(), 0.0f, 1e-6f);
}

TEST_F(TestTorqueCascade, an_attached_stream_receives_the_measured_currents_and_the_mechanical_angle)
{
    foc::CurrentStream stream;
    focTorque->SetCurrentStream(stream);
    stream.Start(1);

    foc::Radians position{ 0.0f };
    focTorque->Calculate({ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } }, position);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->meanId.Value(), 1.0f, 1e-4f);
    EXPECT_NEAR(sample->meanIq.Value(), 0.0f, 1e-4f);
    EXPECT_NEAR(sample->position.Value(), 0.0f, 1e-6f);
}

TEST_F(TestTorqueCascade, the_stream_carries_the_rotor_frame_currents_not_the_stationary_ones)
{
    foc::CurrentStream stream;
    focTorque->SetCurrentStream(stream);
    stream.Start(1);

    const float mechanicalAngle = std::numbers::pi_v<float> / 2.0f / static_cast<float>(polePairs);
    foc::Radians position{ mechanicalAngle };
    focTorque->Calculate({ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } }, position);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->meanId.Value(), 0.0f, 1e-2f);
    EXPECT_NEAR(sample->meanIq.Value(), -1.0f, 1e-2f);
    EXPECT_NEAR(sample->position.Value(), mechanicalAngle, 1e-6f);
}

TEST_F(TestTorqueCascade, enabling_restarts_the_stream_window)
{
    foc::CurrentStream stream;
    focTorque->SetCurrentStream(stream);
    stream.Start(2);

    foc::Radians position{ 0.0f };
    focTorque->Calculate({ foc::Ampere{ 3.0f }, foc::Ampere{ -1.5f }, foc::Ampere{ -1.5f } }, position);
    focTorque->Disable();
    focTorque->Enable();
    focTorque->Calculate({ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } }, position);
    EXPECT_FALSE(stream.Take().has_value());
    focTorque->Calculate({ foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } }, position);

    const auto sample = stream.Take();
    ASSERT_TRUE(sample.has_value());
    EXPECT_NEAR(sample->meanId.Value(), 1.0f, 1e-4f);
}

TEST_F(TestTorqueCascade, the_observed_demand_is_the_q_axis_setpoint)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 1.5f } });

    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), 1.5f, 1e-6f);
}

namespace
{
    class TestTorqueCascadeSpeedLimit
        : public TestTorqueCascade
    {
    public:
        static constexpr float limit{ 10.0f };
        static constexpr std::size_t samples{ 2000 };

        void SetUp() override
        {
            TestTorqueCascade::SetUp();
            focTorque->Disable();
            EXPECT_EQ(focTorque->SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
            focTorque->LimitSpeed(foc::RadiansPerSecond{ limit });
            focTorque->Enable();
        }

        static foc::PhasePwmDutyCycles Spin(foc::TorqueCascade& cascade, float mechanicalSpeed)
        {
            const float step = mechanicalSpeed / 20000.0f;
            float angle = 0.0f;
            foc::PhasePwmDutyCycles result{ hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0) };

            for (std::size_t sample = 0; sample != samples; ++sample)
            {
                foc::Radians position{ angle };
                result = cascade.Calculate(ZeroCurrents(), position);
                angle = math::WrapToPi(angle + step);
            }

            return result;
        }

        foc::PhasePwmDutyCycles Unlimited(float quadrature, float mechanicalSpeed) const
        {
            foc::TorqueCascade reference{ testMaxCurrent };
            reference.Configure(MotorParameters(polePairs));
            EXPECT_EQ(reference.SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
            reference.Enable();
            reference.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ quadrature } });
            return Spin(reference, mechanicalSpeed);
        }

        static void ExpectSameDuty(const foc::PhasePwmDutyCycles& actual, const foc::PhasePwmDutyCycles& expected)
        {
            EXPECT_NEAR(DutyPercent(actual.a), DutyPercent(expected.a), 1e-2f);
            EXPECT_NEAR(DutyPercent(actual.b), DutyPercent(expected.b), 1e-2f);
            EXPECT_NEAR(DutyPercent(actual.c), DutyPercent(expected.c), 1e-2f);
        }
    };
}

TEST_F(TestTorqueCascadeSpeedLimit, a_current_that_pushes_the_rotor_towards_the_limit_is_folded_linearly_from_90_percent)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });

    const auto limited = Spin(*focTorque, 0.95f * limit);

    ExpectSameDuty(limited, Unlimited(0.1f, 0.95f * limit));
    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), 0.1f, 1e-3f);
}

TEST_F(TestTorqueCascadeSpeedLimit, a_reverse_current_that_pushes_a_reversing_rotor_is_folded_as_well)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ -0.2f } });

    const auto limited = Spin(*focTorque, -0.95f * limit);

    ExpectSameDuty(limited, Unlimited(-0.1f, -0.95f * limit));
}

TEST_F(TestTorqueCascadeSpeedLimit, a_current_that_brakes_the_rotor_is_not_folded)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ -0.2f } });

    const auto limited = Spin(*focTorque, 0.95f * limit);

    ExpectSameDuty(limited, Unlimited(-0.2f, 0.95f * limit));
    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), -0.2f, 1e-6f);
}

TEST_F(TestTorqueCascadeSpeedLimit, beyond_the_limit_no_current_pushes_the_rotor)
{
    foc::TorqueCascade reference{ testMaxCurrent };
    reference.Configure(MotorParameters(polePairs));
    EXPECT_EQ(reference.SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
    reference.Enable();

    const float step = 1.2f * limit / 20000.0f;
    float angle = 0.0f;
    foc::PhasePwmDutyCycles limited{ hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0), hal::DutyCycle::FromPercent(0) };
    foc::PhasePwmDutyCycles unlimited{ limited };

    for (std::size_t sample = 0; sample != samples; ++sample)
    {
        if (sample == samples / 2)
            focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });

        foc::Radians position{ angle };
        foc::Radians referencePosition{ angle };
        limited = focTorque->Calculate(ZeroCurrents(), position);
        unlimited = reference.Calculate(ZeroCurrents(), referencePosition);
        angle = math::WrapToPi(angle + step);
    }

    ExpectSameDuty(limited, unlimited);
    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), 0.0f, 1e-6f);
}

TEST_F(TestTorqueCascadeSpeedLimit, below_90_percent_of_the_limit_the_setpoint_is_taken_unchanged)
{
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });

    Spin(*focTorque, 0.85f * limit);

    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), 0.2f, 1e-6f);
}

TEST_F(TestTorqueCascadeSpeedLimit, a_limit_of_zero_folds_nothing)
{
    focTorque->LimitSpeed(foc::RadiansPerSecond{ 0.0f });
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });

    const auto unlimited = Spin(*focTorque, 1.2f * limit);

    ExpectSameDuty(unlimited, Unlimited(0.2f, 1.2f * limit));
    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), 0.2f, 1e-6f);
}

TEST_F(TestTorqueCascadeSpeedLimit, a_limit_set_while_enabled_is_in_force_from_the_next_sample)
{
    focTorque->LimitSpeed(foc::RadiansPerSecond{ 0.0f });
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });
    Spin(*focTorque, 0.95f * limit);

    focTorque->LimitSpeed(foc::RadiansPerSecond{ limit });

    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), 0.1f, 1e-3f);
}

TEST_F(TestTorqueCascadeSpeedLimit, the_limit_is_kept_across_a_reconfiguration_with_other_pole_pairs)
{
    focTorque->Disable();
    focTorque->Configure(MotorParameters(2));
    focTorque->Enable();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });

    Spin(*focTorque, 0.95f * limit);

    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), 0.1f, 1e-3f);
}

TEST_F(TestTorqueCascadeSpeedLimit, a_current_limit_below_the_setpoint_clamps_the_q_axis_reference)
{
    focTorque->LimitSpeed(foc::RadiansPerSecond{ 0.0f });
    focTorque->LimitCurrent(foc::Ampere{ 0.1f });
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });

    const auto limited = Spin(*focTorque, 0.5f * limit);

    ExpectSameDuty(limited, Unlimited(0.1f, 0.5f * limit));
    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), 0.1f, 1e-6f);
}

TEST_F(TestTorqueCascadeSpeedLimit, a_current_limit_clamps_a_reverse_setpoint_as_well)
{
    focTorque->LimitSpeed(foc::RadiansPerSecond{ 0.0f });
    focTorque->LimitCurrent(foc::Ampere{ 0.1f });
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ -0.2f } });

    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), -0.1f, 1e-6f);
}

TEST_F(TestTorqueCascadeSpeedLimit, a_current_limit_above_the_envelope_is_held_at_the_envelope)
{
    focTorque->LimitCurrent(foc::Ampere{ 100.0f });
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 50.0f } });

    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), testMaxCurrent.Value(), 1e-6f);
}

namespace
{
    std::array<int8_t, 48> CoggingWave()
    {
        std::array<int8_t, 48> torque{};
        for (std::size_t bin = 0; bin != torque.size(); ++bin)
            torque[bin] = static_cast<int8_t>(std::lround(127.0f * std::sin(6.0f * (static_cast<float>(bin) + 0.5f) * 2.0f * std::numbers::pi_v<float> / 48.0f)));
        return torque;
    }

    constexpr float coggingScale{ 0.01f / 127.0f };
    const foc::NewtonMeter coggingTorqueConstant{ 0.1f };
    constexpr float offPeakAngle{ 0.03f };
}

TEST_F(TestTorqueCascadeSpeedLimit, a_loaded_cogging_map_adds_the_cogging_current_to_the_setpoint)
{
    const auto torque = CoggingWave();
    focTorque->LimitSpeed(foc::RadiansPerSecond{ 0.0f });
    focTorque->LoadCoggingMap(infra::MakeRange(torque), coggingScale, coggingTorqueConstant);
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    foc::Radians position{ offPeakAngle };
    const auto compensated = focTorque->Calculate(ZeroCurrents(), position);

    foc::CoggingFeedForward feedForward;
    feedForward.Load(infra::MakeRange(torque), coggingScale, coggingTorqueConstant);
    foc::TorqueCascade reference{ testMaxCurrent };
    reference.Configure(MotorParameters(polePairs));
    EXPECT_EQ(reference.SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
    reference.Enable();
    reference.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ feedForward.CurrentAt(offPeakAngle * static_cast<float>(polePairs)) } });
    foc::Radians referencePosition{ offPeakAngle };

    ExpectSameDuty(compensated, reference.Calculate(ZeroCurrents(), referencePosition));
    EXPECT_NEAR(focTorque->ObserveMotion().demandedTorqueCurrent.Value(), 0.0f, 1e-6f);
}

TEST_F(TestTorqueCascadeSpeedLimit, a_cleared_cogging_map_adds_nothing)
{
    const auto torque = CoggingWave();
    focTorque->LoadCoggingMap(infra::MakeRange(torque), coggingScale, coggingTorqueConstant);
    focTorque->ClearCoggingMap();
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.0f } });
    foc::Radians position{ offPeakAngle };

    ExpectCentredDuty(focTorque->Calculate(ZeroCurrents(), position));
}

TEST_F(TestTorqueCascadeSpeedLimit, the_cogging_current_is_held_within_the_current_limit)
{
    const auto torque = CoggingWave();
    focTorque->LimitSpeed(foc::RadiansPerSecond{ 0.0f });
    focTorque->LoadCoggingMap(infra::MakeRange(torque), 127.0f * coggingScale, coggingTorqueConstant);
    focTorque->LimitCurrent(foc::Ampere{ 0.2f });
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });
    foc::Radians position{ offPeakAngle };
    const auto limited = focTorque->Calculate(ZeroCurrents(), position);

    foc::TorqueCascade reference{ testMaxCurrent };
    reference.Configure(MotorParameters(polePairs));
    EXPECT_EQ(reference.SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
    reference.Enable();
    reference.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });
    foc::Radians referencePosition{ offPeakAngle };

    ExpectSameDuty(limited, reference.Calculate(ZeroCurrents(), referencePosition));
}

TEST_F(TestTorqueCascadeSpeedLimit, a_calibration_current_command_is_not_compensated)
{
    const auto torque = CoggingWave();
    focTorque->LoadCoggingMap(infra::MakeRange(torque), coggingScale, coggingTorqueConstant);
    focTorque->Disable();
    focTorque->EnableCurrentCommand();
    foc::Radians position{ offPeakAngle };

    ExpectCentredDuty(focTorque->Calculate(ZeroCurrents(), position));
}

TEST_F(TestTorqueCascade, a_bus_at_half_the_configured_voltage_doubles_the_duty_for_the_same_command)
{
    foc::TorqueCascade nominal{ testMaxCurrent };
    nominal.Configure(MotorParameters(polePairs));
    nominal.SetCurrentTunings(foc::CurrentLoopTunings{});
    nominal.Enable();
    nominal.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 0.2f } });

    focTorque->TrackBusVoltage(foc::Volts{ 12.0f });

    foc::Radians nominalPosition{ 0.0f };
    foc::Radians position{ 0.0f };
    const auto expected = nominal.Calculate(ZeroCurrents(), nominalPosition);
    const auto halved = focTorque->Calculate(ZeroCurrents(), position);

    ASSERT_TRUE(IsOffCentre(expected));
    EXPECT_NEAR(DutyPercent(halved.a) - 50.0f, 2.0f * (DutyPercent(expected.a) - 50.0f), 0.1f);
    EXPECT_NEAR(DutyPercent(halved.b) - 50.0f, 2.0f * (DutyPercent(expected.b) - 50.0f), 0.1f);
    EXPECT_NEAR(DutyPercent(halved.c) - 50.0f, 2.0f * (DutyPercent(expected.c) - 50.0f), 0.1f);
}

TEST_F(TestTorqueCascade, a_bus_at_half_the_configured_voltage_saturates_at_the_same_duties)
{
    foc::TorqueCascade nominal{ testMaxCurrent };
    nominal.Configure(MotorParameters(polePairs));
    nominal.SetCurrentTunings(foc::CurrentLoopTunings{});
    nominal.Enable();
    nominal.SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 10.0f } });
    focTorque->SetCurrentTunings(foc::CurrentLoopTunings{});
    focTorque->SetPoint({ foc::Ampere{ 0.0f }, foc::Ampere{ 10.0f } });

    focTorque->TrackBusVoltage(foc::Volts{ 12.0f });

    foc::Radians nominalPosition{ 0.0f };
    foc::Radians position{ 0.0f };
    const auto expected = nominal.Calculate(ZeroCurrents(), nominalPosition);
    const auto halved = focTorque->Calculate(ZeroCurrents(), position);

    EXPECT_NEAR(DutyPercent(halved.a), DutyPercent(expected.a), 0.1f);
    EXPECT_NEAR(DutyPercent(halved.b), DutyPercent(expected.b), 0.1f);
    EXPECT_NEAR(DutyPercent(halved.c), DutyPercent(expected.c), 0.1f);
}

TEST_F(TestTorqueCascade, the_inverse_park_angle_is_advanced_by_the_duty_delay_and_the_hold_at_a_positive_speed)
{
    constexpr float step = 0.005f;

    ExpectSameDuty(DutyAfterConstantSpeed(1, polePairs, step, 0.0f), DutyAfterConstantSpeed(0, polePairs, step, step));
}

TEST_F(TestTorqueCascade, the_inverse_park_angle_is_advanced_by_the_duty_delay_and_the_hold_at_a_negative_speed)
{
    constexpr float step = -0.005f;

    ExpectSameDuty(DutyAfterConstantSpeed(1, polePairs, step, 0.0f), DutyAfterConstantSpeed(0, polePairs, step, step));
}

TEST_F(TestTorqueCascade, the_advance_moves_the_voltage_once_the_rotor_turns)
{
    constexpr float step = 0.005f;

    EXPECT_GT(std::abs(DutyPercent(DutyAfterConstantSpeed(1, polePairs, step, 0.0f).a) - DutyPercent(DutyAfterConstantSpeed(0, polePairs, step, 0.0f).a)), 0.1f);
}

TEST_F(TestTorqueCascade, a_duty_that_acts_later_leaves_the_voltage_at_standstill_unchanged)
{
    ExpectSameDuty(DutyAfterConstantSpeed(1, polePairs, 0.0f, 0.3f), DutyAfterConstantSpeed(0, polePairs, 0.0f, 0.3f));
}

TEST(TestTorqueCascadeFriction, the_torque_cascade_has_no_friction_feed_forward)
{
    static_assert(!std::is_base_of_v<foc::FrictionCompensable, foc::TorqueCascade>);
    SUCCEED();
}

TEST_F(TestTorqueCascade, each_sample_hands_one_set_of_zero_vector_slopes_to_the_sink)
{
    testing::StrictMock<foc::ZeroVectorSlopeSinkMock> sink;
    focTorque->SetZeroVectorSlopeSink(sink);

    EXPECT_CALL(sink, ZeroVectorSlopes(testing::_)).Times(2);
    foc::Radians position{ 0.0f };
    focTorque->Calculate(ZeroCurrents(), position);
    focTorque->Calculate(ZeroCurrents(), position);
}

TEST(TestTorqueCascadeZeroVectorSlopes, after_each_sample_the_slopes_follow_the_model_at_the_measured_speed)
{
    constexpr float mechanicalStep{ 0.01f };
    const auto parameters = MotorParameters(2);
    foc::TorqueCascade cascade{ foc::Ampere{ 10.0f } };
    testing::StrictMock<foc::ZeroVectorSlopeSinkMock> sink;
    foc::PhaseCurrentSlopes slopes{};
    EXPECT_CALL(sink, ZeroVectorSlopes(testing::_)).WillRepeatedly(testing::SaveArg<0>(&slopes));
    cascade.SetZeroVectorSlopeSink(sink);
    cascade.Configure(parameters);
    cascade.Enable();

    float angle{ 0.0f };
    for (int sample = 0; sample != 400; ++sample)
    {
        angle = static_cast<float>(sample) * mechanicalStep;
        foc::Radians position{ angle };
        cascade.Calculate(ZeroCurrents(), position);
    }

    foc::ZeroVectorSlope reference;
    reference.Configure(parameters);
    const auto electricalAngle = math::WrapToPi(2.0f * angle);
    const auto expected = reference.Next({ 0.0f, 0.0f }, 2.0f * mechanicalStep * 20000.0f, std::cos(electricalAngle), std::sin(electricalAngle));

    EXPECT_NEAR(slopes.a.Value(), expected.a.Value(), 20.0f);
    EXPECT_NEAR(slopes.b.Value(), expected.b.Value(), 20.0f);
    EXPECT_NEAR(slopes.c.Value(), expected.c.Value(), 20.0f);
    EXPECT_GT(std::abs(expected.a.Value()) + std::abs(expected.b.Value()), 1000.0f);
}
