#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include <cmath>
#include <gmock/gmock.h>
#include <numbers>

namespace
{
    using namespace testing;

    constexpr uint32_t baseFrequency = 20000;
    constexpr uint32_t outerLoopFrequency = 1000;
    constexpr float samplingPeriod = 1.0f / static_cast<float>(baseFrequency);
    constexpr float torqueConstant = 0.1f;
    constexpr float polePairs = 7.0f;

    constexpr float trueInertia = 2.0e-4f;
    constexpr float trueFriction = 1.5e-3f;
    constexpr float trueCoulomb = 5.0e-3f;

    services::MechanicalParametersIdentification::Config DefaultConfig()
    {
        auto config = services::MechanicalParametersIdentification::Config{};
        config.timeout = std::chrono::seconds{ 5 };
        return config;
    }

    // Builds the phase currents whose q component, after the service's own Clarke/Park at this rotor
    // position, is exactly the one the requested torque needs. The unit round trip removes the gain the
    // lookup-table trigonometry introduces, so the observation the estimator sees is the intended one.
    foc::PhaseCurrents CurrentsProducing(float torque, float position)
    {
        const auto electricalAngle = position * polePairs;
        const auto cosine = foc::FastTrigonometry::Cosine(electricalAngle);
        const auto sine = foc::FastTrigonometry::Sine(electricalAngle);
        const foc::ClarkePark transform;

        const auto unitGain = transform.Forward(transform.Inverse(foc::RotatingFrame{ 0.0f, 1.0f }, cosine, sine), cosine, sine).q;
        const auto phases = transform.Inverse(foc::RotatingFrame{ 0.0f, torque / torqueConstant / unitGain }, cosine, sine);

        return foc::PhaseCurrents{ foc::Ampere{ phases.a }, foc::Ampere{ phases.b }, foc::Ampere{ phases.c } };
    }

    class MechanicalParametersIdentificationTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        void TearDown() override
        {
            ExecuteAllActions();
        }

        StrictMock<foc::SpeedCommandableMock> controllerMock;
        StrictMock<foc::ControllableMock> driveMock;
        StrictMock<foc::PhaseCurrentsObservableMock> observableMock;
        StrictMock<drivers::ThreePhaseInverterMock> driverMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        infra::Execute setFrequencies{ [this]()
            {
                EXPECT_CALL(controllerMock, SpeedCommandFrequency()).WillRepeatedly(Return(hal::Hertz{ outerLoopFrequency }));
                EXPECT_CALL(driverMock, BaseFrequency()).WillRepeatedly(Return(hal::Hertz{ baseFrequency }));
            } };
        infra::WithSharedAccess<services::MechanicalParametersIdentificationImpl> identification{ controllerMock, driveMock, observableMock, driverMock, encoderMock };

        float position{ 0.0f };
        float previousPosition{ 0.0f };
        float previousSpeed{ 0.0f };
        float lastSpeed{ 0.0f };
        float lastAcceleration{ 0.0f };
        std::size_t sampleIndex{ 0 };

        void ExpectRunStarted()
        {
            EXPECT_CALL(observableMock, RegisterPhaseCurrentsObserver(_))
                .WillOnce(Invoke([this](const infra::Function<void(const foc::PhaseCurrents&)>& observer)
                    {
                        observableMock.StoreObserver(observer);
                    }));
            EXPECT_CALL(driveMock, Start());
            EXPECT_CALL(controllerMock, EnableSpeedCommand());
        }

        void ExpectDriveReleased(Cardinality times = Exactly(1))
        {
            EXPECT_CALL(driveMock, Stop()).Times(times);
            EXPECT_CALL(controllerMock, DisableSpeedCommand()).Times(times);
            EXPECT_CALL(observableMock, UnregisterPhaseCurrentsObserver())
                .Times(times)
                .WillRepeatedly(Invoke([this]()
                    {
                        observableMock.ReleaseObserver();
                    }));
        }

        void GivenEncoderFollowsTheProfile()
        {
            EXPECT_CALL(encoderMock, Read())
                .WillRepeatedly(Invoke([this]()
                    {
                        return foc::Radians{ position };
                    }));
        }

        // Mirrors the service's own double finite difference so the observation handed to the estimator is
        // consistent with the motion the encoder reports.
        void AdvanceProfile()
        {
            constexpr float samplesPerCycle = 1000.0f;
            constexpr float speedAmplitude = 9.0f;
            const auto speedCommand = speedAmplitude * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(sampleIndex) / samplesPerCycle));
            ++sampleIndex;

            previousPosition = position;
            position += speedCommand * samplingPeriod;

            const auto speed = (position - previousPosition) / samplingPeriod;
            lastAcceleration = (speed - previousSpeed) / samplingPeriod;
            previousSpeed = speed;
            lastSpeed = speed;
        }

        void PublishConsistentSample()
        {
            AdvanceProfile();
            const auto torque = trueCoulomb + trueInertia * lastAcceleration + trueFriction * lastSpeed;
            observableMock.Publish(CurrentsProducing(torque, position));
        }

        void PublishCurrents(float a = 1.0f, float b = -0.5f, float c = -0.5f)
        {
            observableMock.Publish(foc::PhaseCurrents{ foc::Ampere{ a }, foc::Ampere{ b }, foc::Ampere{ c } });
        }
    };
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_starts_the_drive_and_sets_target_speed)
{
    auto config = DefaultConfig();

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(config.targetSpeed));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config, [](auto, auto) {});

    EXPECT_TRUE(observableMock.HasObserver());

    ExpectDriveReleased();
    identification->Abort();
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_never_claims_the_inverter_callback_slot)
{
    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _)).Times(0);

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, DefaultConfig(), [](auto, auto) {});

    ExpectDriveReleased();
    identification->Abort();
}

TEST_F(MechanicalParametersIdentificationTest, the_excitation_trajectory_alternates_between_two_speed_levels)
{
    auto config = DefaultConfig();
    config.dwellTime = std::chrono::milliseconds{ 100 };

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();

    InSequence sequence;
    EXPECT_CALL(controllerMock, CommandSpeed(config.targetSpeed));
    EXPECT_CALL(controllerMock, CommandSpeed(config.dwellSpeed));
    EXPECT_CALL(controllerMock, CommandSpeed(config.targetSpeed));
    EXPECT_CALL(controllerMock, CommandSpeed(config.dwellSpeed));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config, [](auto, auto) {});

    ForwardTime(std::chrono::milliseconds{ 310 });

    ExpectDriveReleased();
    identification->Abort();
}

TEST_F(MechanicalParametersIdentificationTest, a_config_without_a_usable_trajectory_is_rejected_immediately)
{
    auto config = DefaultConfig();
    config.dwellSpeed = config.targetSpeed;

    bool fired = false;

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config,
        [&fired](auto friction, auto inertia)
        {
            fired = true;
            EXPECT_FALSE(friction.has_value());
            EXPECT_FALSE(inertia.has_value());
        });

    EXPECT_TRUE(fired);
    EXPECT_FALSE(identification->IsRunning());
}

TEST_F(MechanicalParametersIdentificationTest, a_config_whose_envelope_is_not_positive_is_rejected_immediately)
{
    auto config = DefaultConfig();
    config.maxCurrent = foc::Ampere{ 0.0f };

    bool fired = false;

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config,
        [&fired](auto, auto)
        {
            fired = true;
        });

    EXPECT_TRUE(fired);
    EXPECT_FALSE(identification->IsRunning());
}

TEST_F(MechanicalParametersIdentificationTest, a_run_without_a_torque_constant_is_rejected_immediately)
{
    bool fired = false;

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ 0.0f }, 7, DefaultConfig(),
        [&fired](auto, auto)
        {
            fired = true;
        });

    EXPECT_TRUE(fired);
    EXPECT_FALSE(identification->IsRunning());
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_timeout_calls_done_with_nullopt)
{
    auto config = DefaultConfig();
    config.timeout = std::chrono::milliseconds{ 500 };
    config.dwellTime = std::chrono::milliseconds{ 250 };

    std::optional<foc::NewtonMeterSecondPerRadian> resultFriction = foc::NewtonMeterSecondPerRadian{ 99.0f };
    std::optional<foc::NewtonMeterSecondSquared> resultInertia = foc::NewtonMeterSecondSquared{ 99.0f };

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_)).Times(AnyNumber());
    ExpectDriveReleased();

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config, [&](auto friction, auto inertia)
        {
            resultFriction = friction;
            resultInertia = inertia;
        });

    ForwardTime(std::chrono::milliseconds{ 500 } + std::chrono::milliseconds{ 1 });

    EXPECT_FALSE(resultFriction.has_value());
    EXPECT_FALSE(resultInertia.has_value());
}

TEST_F(MechanicalParametersIdentificationTest, concurrent_estimate_friction_call_is_rejected_immediately)
{
    bool firstCallbackCalled = false;
    bool secondCallbackCalled = false;

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, DefaultConfig(),
        [&](auto, auto)
        {
            firstCallbackCalled = true;
        });

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, DefaultConfig(),
        [&](auto friction, auto inertia)
        {
            secondCallbackCalled = true;
            EXPECT_FALSE(friction.has_value());
            EXPECT_FALSE(inertia.has_value());
        });

    EXPECT_FALSE(firstCallbackCalled);
    EXPECT_TRUE(secondCallbackCalled);

    ExpectDriveReleased();
    identification->Abort();
}

TEST_F(MechanicalParametersIdentificationTest, abort_releases_the_drive_and_drops_the_completion)
{
    bool fired = false;

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, DefaultConfig(), [&](auto, auto)
        {
            fired = true;
        });

    ExpectDriveReleased();
    identification->Abort();

    EXPECT_FALSE(fired);
    EXPECT_FALSE(observableMock.HasObserver());

    ForwardTime(std::chrono::seconds{ 10 });
    EXPECT_FALSE(fired);
}

TEST_F(MechanicalParametersIdentificationTest, abort_without_a_run_in_flight_is_a_no_op)
{
    identification->Abort();
}

TEST_F(MechanicalParametersIdentificationTest, is_running_returns_false_when_not_started)
{
    EXPECT_FALSE(identification->IsRunning());
}

TEST_F(MechanicalParametersIdentificationTest, is_running_returns_true_while_estimating)
{
    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, DefaultConfig(), [](auto, auto) {});

    EXPECT_TRUE(identification->IsRunning());

    ExpectDriveReleased();
    identification->Abort();
    EXPECT_FALSE(identification->IsRunning());
}

TEST_F(MechanicalParametersIdentificationTest, a_run_that_has_not_converged_keeps_the_drive_turning)
{
    auto config = DefaultConfig();
    config.timeout = std::chrono::seconds{ 60 };

    struct Result
    {
        bool fired = false;
        std::optional<foc::NewtonMeterSecondPerRadian> friction;
        std::optional<foc::NewtonMeterSecondSquared> inertia;
    } outcome;

    std::size_t callIndex = 0;
    EXPECT_CALL(encoderMock, Read())
        .WillRepeatedly(Invoke([&]()
            {
                const std::size_t index = (callIndex == 0) ? 0u : (callIndex + 1) / 2;
                ++callIndex;
                const float pos = static_cast<float>(index) * static_cast<float>(index) * 1e-5f;
                return foc::Radians{ pos };
            }));

    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_)).Times(AnyNumber());
    ExpectDriveReleased(AtMost(1));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config,
        [&outcome](auto f, auto i)
        {
            outcome.fired = true;
            outcome.friction = f;
            outcome.inertia = i;
        });

    for (std::size_t i = 0; i != 2000 && !outcome.fired; ++i)
    {
        PublishCurrents();
        ExecuteAllActions();
    }

    EXPECT_EQ(outcome.fired, !observableMock.HasObserver());

    if (!outcome.fired)
    {
        ExpectDriveReleased();
        identification->Abort();
    }
}

TEST_F(MechanicalParametersIdentificationTest, a_consistent_excited_run_reports_the_identified_mechanics)
{
    auto config = DefaultConfig();
    config.timeout = std::chrono::seconds{ 60 };

    struct Outcome
    {
        bool fired = false;
        std::optional<foc::NewtonMeterSecondPerRadian> friction;
        std::optional<foc::NewtonMeterSecondSquared> inertia;
    } outcome;

    GivenEncoderFollowsTheProfile();
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_)).Times(AnyNumber());
    ExpectDriveReleased();

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config,
        [&outcome](auto b, auto j)
        {
            outcome.fired = true;
            outcome.friction = b;
            outcome.inertia = j;
        });

    for (std::size_t i = 0; i != 4000 && !outcome.fired; ++i)
    {
        PublishConsistentSample();
        ExecuteAllActions();
    }

    ASSERT_TRUE(outcome.fired);
    ASSERT_TRUE(outcome.inertia.has_value());
    ASSERT_TRUE(outcome.friction.has_value());
    EXPECT_NEAR(outcome.inertia->Value(), trueInertia, trueInertia * 0.2f);
    EXPECT_NEAR(outcome.friction->Value(), trueFriction, trueFriction * 0.2f);
    EXPECT_FALSE(observableMock.HasObserver());
}

TEST_F(MechanicalParametersIdentificationTest, a_sample_above_the_current_envelope_ends_the_run_without_an_estimate)
{
    auto config = DefaultConfig();
    config.timeout = std::chrono::seconds{ 60 };
    config.maxCurrent = foc::Ampere{ 2.0f };

    std::optional<foc::NewtonMeterSecondPerRadian> friction = foc::NewtonMeterSecondPerRadian{ 99.0f };
    bool fired = false;

    GivenEncoderFollowsTheProfile();
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_)).Times(AnyNumber());
    ExpectDriveReleased();

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config,
        [&](auto b, auto)
        {
            fired = true;
            friction = b;
        });

    AdvanceProfile();
    PublishCurrents(5.0f, -2.5f, -2.5f);
    ExecuteAllActions();

    EXPECT_TRUE(fired);
    EXPECT_FALSE(friction.has_value());
    EXPECT_FALSE(observableMock.HasObserver());
}

TEST_F(MechanicalParametersIdentificationTest, a_rotor_faster_than_the_speed_limit_ends_the_run_without_an_estimate)
{
    auto config = DefaultConfig();
    config.timeout = std::chrono::seconds{ 60 };
    config.maxSpeed = foc::RadiansPerSecond{ 60.0f };

    std::optional<foc::NewtonMeterSecondSquared> inertia = foc::NewtonMeterSecondSquared{ 99.0f };
    bool fired = false;

    EXPECT_CALL(encoderMock, Read())
        .WillRepeatedly(Invoke([this]()
            {
                return foc::Radians{ position };
            }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_)).Times(AnyNumber());
    ExpectDriveReleased();

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config,
        [&](auto, auto j)
        {
            fired = true;
            inertia = j;
        });

    position += 100.0f * samplingPeriod;
    PublishCurrents(0.1f, -0.05f, -0.05f);
    ExecuteAllActions();

    EXPECT_TRUE(fired);
    EXPECT_FALSE(inertia.has_value());
    EXPECT_FALSE(observableMock.HasObserver());
}

TEST_F(MechanicalParametersIdentificationTest, a_rotor_held_at_a_constant_speed_never_converges)
{
    auto config = DefaultConfig();
    config.timeout = std::chrono::seconds{ 60 };

    bool fired = false;

    EXPECT_CALL(encoderMock, Read())
        .WillRepeatedly(Invoke([this]()
            {
                return foc::Radians{ position };
            }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_)).Times(AnyNumber());

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ torqueConstant }, 7, config,
        [&](auto, auto)
        {
            fired = true;
        });

    for (std::size_t i = 0; i != 2000 && !fired; ++i)
    {
        position += 10.0f * samplingPeriod;
        PublishCurrents(0.5f, -0.25f, -0.25f);
        ExecuteAllActions();
    }

    EXPECT_FALSE(fired);
    EXPECT_TRUE(observableMock.HasObserver());

    ExpectDriveReleased();
    identification->Abort();
}
