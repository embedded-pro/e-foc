#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/flux_linkage_ident/FluxLinkageIdentificationImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <gmock/gmock.h>
#include <optional>

namespace
{
    using namespace testing;
    using Outcome = services::FluxLinkageIdentification::Outcome;

    constexpr float fluxLinkage = 0.0046f;
    constexpr std::size_t polePairs = 4;
    constexpr float resistance = 1.1f;
    constexpr float torqueConstant = 1.5f * static_cast<float>(polePairs) * fluxLinkage;
    constexpr float inertia = 12.4e-6f;
    constexpr float tickSeconds = 0.001f;

    struct Rotor
    {
        float speed{ 0.0f };
        float current{ 0.0f };
        float dryFriction{ 3.0e-3f };
        float voltageOffset{ 0.0f };
        float torqueScale{ 1.0f };
        float residualCurrent{ 0.0f };
        float voltage{ 0.0f };

        foc::MotionObservation Step()
        {
            const auto flowing = current + residualCurrent;
            const auto drive = torqueScale * torqueConstant * flowing;

            if (speed != 0.0f || std::abs(drive) > dryFriction)
            {
                const auto direction = speed != 0.0f ? std::copysign(1.0f, speed) : std::copysign(1.0f, drive);
                const auto next = speed + (drive - direction * dryFriction) / inertia * tickSeconds;
                speed = (speed != 0.0f && next * speed < 0.0f) ? 0.0f : next;
            }

            voltage = fluxLinkage * static_cast<float>(polePairs) * speed + resistance * flowing + voltageOffset;

            return foc::MotionObservation{ foc::RadiansPerSecond{ speed }, foc::Ampere{ flowing }, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 0.0f } };
        }
    };

    class TestFluxLinkageIdentification
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectMotion()
        {
            EXPECT_CALL(driveMock, Start());
            EXPECT_CALL(controllerMock, EnableCurrentCommand());
            EXPECT_CALL(controllerMock, CommandCurrent(_)).WillRepeatedly(Invoke([this](foc::Ampere current)
                {
                    rotor.current = current.Value();
                }));
            EXPECT_CALL(motionMock, ObserveMotion()).WillRepeatedly(Invoke([this]()
                {
                    return rotor.Step();
                }));
            ExpectVoltage();
            EXPECT_CALL(controllerMock, DisableCurrentCommand());
            EXPECT_CALL(driveMock, Stop());
        }

        void ExpectVoltage()
        {
            EXPECT_CALL(controllerMock, QuadratureVoltage()).WillRepeatedly(Invoke([this]()
                {
                    return foc::Volts{ rotor.voltage };
                }));
        }

        void Identify(std::size_t pairs = polePairs)
        {
            identification.Identify(pairs, foc::Ohm{ resistance }, services::FluxLinkageIdentification::Config{}, [this](const auto& outcome)
                {
                    result = outcome;
                });
        }

        Rotor rotor;
        StrictMock<foc::CurrentCommandableMock> controllerMock;
        StrictMock<foc::ControllableMock> driveMock;
        StrictMock<foc::FocTorqueMock> motionMock;
        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        services::FluxLinkageIdentificationImpl identification{ controllerMock, driveMock, motionMock, inverterMock };
        std::optional<services::FluxLinkageIdentification::Result> result;
    };
}

TEST_F(TestFluxLinkageIdentification, identifies_the_flux_linkage_from_the_back_emf_while_the_rotor_coasts)
{
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 5 });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::identified);
    EXPECT_NEAR(result->fluxLinkage.Value(), fluxLinkage, 0.01f * fluxLinkage);
    EXPECT_GE(result->samples, services::FluxLinkageIdentification::Config{}.requiredSamples);
    EXPECT_FALSE(identification.IsRunning());
}

TEST_F(TestFluxLinkageIdentification, a_constant_voltage_offset_does_not_bias_the_flux_linkage)
{
    rotor.voltageOffset = 0.05f;
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 5 });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::identified);
    EXPECT_NEAR(result->fluxLinkage.Value(), fluxLinkage, 0.01f * fluxLinkage);
    EXPECT_NEAR(result->intercept.Value(), 0.05f, 0.005f);
}

TEST_F(TestFluxLinkageIdentification, a_rotor_that_barely_slows_is_braked_to_a_low_speed_so_the_fit_spans_speeds)
{
    rotor.dryFriction = 1.0e-5f;
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 6 });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::identified);
    EXPECT_NEAR(result->fluxLinkage.Value(), fluxLinkage, 0.01f * fluxLinkage);
}

TEST_F(TestFluxLinkageIdentification, a_rotor_the_current_law_keeps_driving_is_slowed_and_recorded_as_it_speeds_up)
{
    rotor.dryFriction = 1.0e-4f;
    rotor.residualCurrent = 0.1f;
    float peakSpeed = 0.0f;
    ExpectMotion();
    EXPECT_CALL(motionMock, ObserveMotion()).WillRepeatedly(Invoke([this, &peakSpeed]()
        {
            const auto observation = rotor.Step();
            peakSpeed = std::max(peakSpeed, std::abs(rotor.speed));
            return observation;
        }));
    Identify();
    ForwardTime(std::chrono::seconds{ 6 });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::identified);
    EXPECT_NEAR(result->fluxLinkage.Value(), fluxLinkage, 0.01f * fluxLinkage);
    EXPECT_LT(peakSpeed, services::FluxLinkageIdentification::Config{}.maxSpeed.Value());
}

TEST_F(TestFluxLinkageIdentification, brakes_the_rotor_to_rest_before_reporting)
{
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 5 });

    ASSERT_TRUE(result.has_value());
    EXPECT_LE(std::abs(rotor.speed), services::FluxLinkageIdentification::Config{}.stoppedSpeed.Value() + 2.0f);
    EXPECT_EQ(rotor.current, 0.0f);
}

TEST_F(TestFluxLinkageIdentification, the_spin_current_stays_within_a_quarter_of_the_drive_current)
{
    ON_CALL(inverterMock, MaxCurrentSupported()).WillByDefault(Return(foc::Ampere{ 1.0f }));
    services::FluxLinkageIdentificationImpl limited{ controllerMock, driveMock, motionMock, inverterMock };

    float peakCurrent = 0.0f;
    EXPECT_CALL(driveMock, Start());
    EXPECT_CALL(controllerMock, EnableCurrentCommand());
    EXPECT_CALL(controllerMock, CommandCurrent(_)).WillRepeatedly(Invoke([&](foc::Ampere current)
        {
            rotor.current = current.Value();
            peakCurrent = std::max(peakCurrent, std::abs(current.Value()));
        }));
    EXPECT_CALL(motionMock, ObserveMotion()).WillRepeatedly(Invoke([this]()
        {
            return rotor.Step();
        }));
    ExpectVoltage();
    EXPECT_CALL(controllerMock, DisableCurrentCommand());
    EXPECT_CALL(driveMock, Stop());

    limited.Identify(polePairs, foc::Ohm{ resistance }, services::FluxLinkageIdentification::Config{}, [this](const auto& outcome)
        {
            result = outcome;
        });
    ForwardTime(std::chrono::seconds{ 6 });

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(peakCurrent, 0.25f, 1e-6f);
}

TEST_F(TestFluxLinkageIdentification, a_rotor_running_away_is_stopped_and_reported_as_overspeed)
{
    rotor.torqueScale = 100.0f;
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 1 });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::overspeed);
    EXPECT_FALSE(identification.IsRunning());
}

TEST_F(TestFluxLinkageIdentification, a_rotor_that_never_reaches_the_release_speed_is_not_identified)
{
    rotor.dryFriction = 1.0f;
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 6 });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::notIdentified);
    EXPECT_EQ(result->samples, 0u);
}

TEST_F(TestFluxLinkageIdentification, abort_stops_the_motion_and_drops_the_completion)
{
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::milliseconds{ 20 });

    identification.Abort();
    ForwardTime(std::chrono::seconds{ 5 });

    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(identification.IsRunning());
}

TEST_F(TestFluxLinkageIdentification, an_unusable_request_is_refused_without_moving_the_rotor)
{
    Identify(0);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::notIdentified);
    EXPECT_FALSE(identification.IsRunning());
}
