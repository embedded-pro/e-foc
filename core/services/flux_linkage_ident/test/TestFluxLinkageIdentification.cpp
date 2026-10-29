#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/flux_linkage_ident/FluxLinkageIdentificationImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <algorithm>
#include <array>
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
        float backEmfScale{ 1.0f };
        float torqueScale{ 1.0f };
        float residualCurrent{ 0.0f };
        float voltage{ 0.0f };
        float rotorInertia{ inertia };
        bool late{ false };
        float commanded{ 0.0f };

        void Command(float ampere)
        {
            commanded = ampere;
            if (!late)
                current = ampere;
        }

        foc::MotionObservation Step()
        {
            const auto reported = speed;
            const auto flowing = current + residualCurrent;
            const auto drive = torqueScale * torqueConstant * flowing;

            if (speed != 0.0f || std::abs(drive) > dryFriction)
            {
                const auto direction = speed != 0.0f ? std::copysign(1.0f, speed) : std::copysign(1.0f, drive);
                const auto next = speed + (drive - direction * dryFriction) / rotorInertia * tickSeconds;
                speed = (speed != 0.0f && next * speed < 0.0f) ? 0.0f : next;
            }

            voltage = backEmfScale * fluxLinkage * static_cast<float>(polePairs) * speed + resistance * flowing + voltageOffset;
            if (late)
                current = commanded;

            return foc::MotionObservation{ foc::RadiansPerSecond{ late ? reported : speed }, foc::Ampere{ flowing }, foc::RadiansPerSecond{ 0.0f }, foc::Radians{ 0.0f } };
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
                    rotor.Command(current.Value());
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

        void Identify(std::size_t pairs = polePairs, const services::FluxLinkageIdentification::Config& config = {}, float ohm = resistance)
        {
            identification.Identify(pairs, foc::Ohm{ ohm }, config, [this](const auto& outcome)
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

TEST_F(TestFluxLinkageIdentification, a_fast_rotor_seen_a_period_late_is_released_early_enough_to_stay_below_the_speed_limit)
{
    for (const float rotorInertia : { 3.0e-6f, 3.3e-6f, 3.6e-6f, 3.9e-6f, 4.2e-6f })
    {
        SCOPED_TRACE(rotorInertia);
        rotor = Rotor{};
        rotor.rotorInertia = rotorInertia;
        rotor.dryFriction = 3.0e-4f;
        rotor.late = true;
        result.reset();
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
        EXPECT_LT(peakSpeed, services::FluxLinkageIdentification::Config{}.releaseSpeed.Value() + 5.0f);
    }
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

TEST_F(TestFluxLinkageIdentification, an_unusable_configuration_or_resistance_is_refused_without_moving_the_rotor)
{
    using Config = services::FluxLinkageIdentification::Config;
    const std::array<void (*)(Config&), 13> unusable{
        [](Config& config)
        {
            config.spinCurrent = foc::Ampere{ 0.0f };
        },
        [](Config& config)
        {
            config.lowestSpeed = foc::RadiansPerSecond{ 0.0f };
        },
        [](Config& config)
        {
            config.lowestSpeed = config.releaseSpeed;
        },
        [](Config& config)
        {
            config.releaseSpeed = config.maxSpeed;
        },
        [](Config& config)
        {
            config.stoppedSpeed = foc::RadiansPerSecond{ -1.0f };
        },
        [](Config& config)
        {
            config.requiredSamples = 9;
        },
        [](Config& config)
        {
            config.maxCoasts = 0;
        },
        [](Config& config)
        {
            config.maxRelativeError = 0.0f;
        },
        [](Config& config)
        {
            config.maxInterceptFraction = 0.0f;
        },
        [](Config& config)
        {
            config.pointPeriod = std::chrono::microseconds{ 500 };
        },
        [](Config& config)
        {
            config.coastLimit = infra::Duration::zero();
        },
        [](Config& config)
        {
            config.brakeLimit = infra::Duration::zero();
        },
        [](Config& config)
        {
            config.timeout = infra::Duration::zero();
        },
    };

    for (const auto& makeUnusable : unusable)
    {
        Config config;
        makeUnusable(config);
        result.reset();
        Identify(polePairs, config);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->outcome, Outcome::notIdentified);
        EXPECT_FALSE(identification.IsRunning());
    }

    result.reset();
    Identify(polePairs, {}, 0.0f);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::notIdentified);
}

TEST_F(TestFluxLinkageIdentification, a_second_request_while_running_is_refused_and_leaves_the_first_running)
{
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::milliseconds{ 20 });

    std::optional<services::FluxLinkageIdentification::Result> second;
    identification.Identify(polePairs, foc::Ohm{ resistance }, services::FluxLinkageIdentification::Config{}, [&second](const auto& outcome)
        {
            second = outcome;
        });

    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->outcome, Outcome::notIdentified);
    EXPECT_TRUE(identification.IsRunning());

    ForwardTime(std::chrono::seconds{ 5 });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::identified);
}

TEST_F(TestFluxLinkageIdentification, abort_without_a_measurement_does_nothing)
{
    identification.Abort();

    EXPECT_FALSE(identification.IsRunning());
}

TEST_F(TestFluxLinkageIdentification, a_rotor_already_turning_backwards_is_recorded_slowed_and_braked_in_that_direction)
{
    rotor.dryFriction = 1.0e-4f;
    rotor.speed = -45.0f;
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 6 });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::identified);
    EXPECT_NEAR(result->fluxLinkage.Value(), fluxLinkage, 0.01f * fluxLinkage);
    EXPECT_LE(std::abs(rotor.speed), services::FluxLinkageIdentification::Config{}.stoppedSpeed.Value() + 2.0f);
}

TEST_F(TestFluxLinkageIdentification, a_large_constant_voltage_offset_is_not_identified)
{
    rotor.voltageOffset = 0.5f;
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 5 });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::notIdentified);
    EXPECT_NEAR(result->intercept.Value(), 0.5f, 0.05f);
}

TEST_F(TestFluxLinkageIdentification, a_flux_linkage_outside_the_parameter_limits_is_not_identified)
{
    rotor.backEmfScale = 3000.0f;
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 5 });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::notIdentified);
    EXPECT_GT(result->fluxLinkage.Value(), 10.0f);
}

TEST_F(TestFluxLinkageIdentification, a_rotor_too_weak_to_stop_in_time_still_reports_and_stops_the_drive)
{
    rotor.dryFriction = 1.0e-4f;
    rotor.torqueScale = 0.05f;
    ExpectMotion();
    Identify();
    ForwardTime(std::chrono::seconds{ 8 });

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(identification.IsRunning());
}
