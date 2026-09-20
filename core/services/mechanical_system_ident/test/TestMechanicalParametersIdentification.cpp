#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/foc/math/AngleWrap.hpp"
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
        infra::Execute setOuterLoopFrequency{ [this]()
            {
                EXPECT_CALL(controllerMock, SpeedCommandFrequency()).WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(driverMock, BaseFrequency()).WillRepeatedly(Return(hal::Hertz{ 10000 }));
            } };
        infra::WithSharedAccess<services::MechanicalParametersIdentificationImpl> identification{ controllerMock, driveMock, observableMock, driverMock, encoderMock };

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

        void PublishCurrents(float a = 1.0f, float b = -0.5f, float c = -0.5f)
        {
            observableMock.Publish(foc::PhaseCurrents{ foc::Ampere{ a }, foc::Ampere{ b }, foc::Ampere{ c } });
        }

        struct MotorModel
        {
            float inertia{ 1e-3f };
            float friction{ 5e-3f };
            float coulomb{ 2e-2f };
            float torqueConstant{ 0.1f };
            std::size_t polePairs{ 1 };
        };

        class Excitation
        {
        public:
            explicit Excitation(const MotorModel& model)
                : model(model)
            {}

            foc::Radians Position() const
            {
                return foc::Radians{ amplitude * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * frequency * static_cast<float>(sample) * samplingPeriod)) };
            }

            void Advance()
            {
                ++sample;
                const auto position = Position().Value();
                const auto speed = foc::detail::PositionWithWrapAround(position - previousPosition) / samplingPeriod;
                const auto acceleration = (speed - previousSpeed) / samplingPeriod;
                previousPosition = position;
                previousSpeed = speed;
                torque = model.coulomb + model.inertia * acceleration + model.friction * speed;
            }

            foc::PhaseCurrents Currents() const
            {
                const auto electricalAngle = Position().Value() * static_cast<float>(model.polePairs);
                const auto cosine = foc::FastTrigonometry::Cosine(electricalAngle);
                const auto sine = foc::FastTrigonometry::Sine(electricalAngle);
                const auto quadrature = torque / model.torqueConstant / (cosine * cosine + sine * sine);
                const auto phases = foc::ClarkePark{}.Inverse(foc::RotatingFrame{ 0.0f, quadrature }, cosine, sine);
                return foc::PhaseCurrents{ foc::Ampere{ phases.a }, foc::Ampere{ phases.b }, foc::Ampere{ phases.c } };
            }

        private:
            MotorModel model;
            float samplingPeriod{ 1.0f / 10000.0f };
            float amplitude{ 0.5f };
            float frequency{ 10.0f };
            std::size_t sample{ 0 };
            float previousPosition{ 0.0f };
            float previousSpeed{ 0.0f };
            float torque{ 0.0f };
        };

        void ExpectEncoderToFollow(Excitation& excitation)
        {
            EXPECT_CALL(encoderMock, Read()).WillRepeatedly(Invoke([&excitation]()
                {
                    return excitation.Position();
                }));
        }

        void PublishExcitation(Excitation& excitation, std::size_t samples)
        {
            for (std::size_t i = 0; i != samples; ++i)
            {
                excitation.Advance();
                observableMock.Publish(excitation.Currents());
            }
        }

        static constexpr std::size_t samplesUntilConvergence{ 1000 };
    };
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_starts_the_drive_and_sets_target_speed)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 52.36f },
        0.998f,
        std::chrono::seconds{ 1 }
    };

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(foc::RadiansPerSecond{ 52.36f }));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [](auto, auto) {});

    EXPECT_TRUE(observableMock.HasObserver());
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_never_claims_the_inverter_callback_slot)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::seconds{ 1 }
    };

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _)).Times(0);

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [](auto, auto) {});
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_timeout_calls_done_with_nullopt)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 52.36f },
        0.998f,
        std::chrono::milliseconds{ 500 }
    };

    std::optional<foc::NewtonMeterSecondPerRadian> resultFriction = foc::NewtonMeterSecondPerRadian{ 99.0f };
    std::optional<foc::NewtonMeterSecondSquared> resultInertia = foc::NewtonMeterSecondSquared{ 99.0f };

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(foc::RadiansPerSecond{ 52.36f }));
    ExpectDriveReleased();

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [&](auto friction, auto inertia)
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
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::seconds{ 5 }
    };

    bool firstCallbackCalled = false;
    bool secondCallbackCalled = false;

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
        [&](auto, auto)
        {
            firstCallbackCalled = true;
        });

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
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
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::seconds{ 5 }
    };

    bool fired = false;

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [&](auto, auto)
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
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::seconds{ 5 }
    };

    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [](auto, auto) {});

    EXPECT_TRUE(identification->IsRunning());

    ExpectDriveReleased();
    identification->Abort();
    EXPECT_FALSE(identification->IsRunning());
}

TEST_F(MechanicalParametersIdentificationTest, a_run_that_has_not_converged_keeps_the_drive_turning)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::seconds{ 60 }
    };

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
                const std::size_t sampleIndex = (callIndex == 0) ? 0u : (callIndex + 1) / 2;
                ++callIndex;
                const float pos = static_cast<float>(sampleIndex) * static_cast<float>(sampleIndex) * 1e-5f;
                return foc::Radians{ pos };
            }));

    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));
    ExpectDriveReleased(AtMost(1));

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
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

TEST_F(MechanicalParametersIdentificationTest, a_converged_run_completes_with_the_identified_parameters)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::seconds{ 60 }
    };
    MotorModel model;
    Excitation excitation{ model };

    struct Result
    {
        bool fired = false;
        std::optional<foc::NewtonMeterSecondPerRadian> friction;
        std::optional<foc::NewtonMeterSecondSquared> inertia;
    } outcome;

    ExpectEncoderToFollow(excitation);
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));
    ExpectDriveReleased();

    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ model.torqueConstant }, model.polePairs, config,
        [&outcome](auto f, auto i)
        {
            outcome.fired = true;
            outcome.friction = f;
            outcome.inertia = i;
        });

    PublishExcitation(excitation, samplesUntilConvergence);
    ExecuteAllActions();

    ASSERT_TRUE(outcome.fired);
    ASSERT_TRUE(outcome.friction.has_value());
    ASSERT_TRUE(outcome.inertia.has_value());
    EXPECT_NEAR(outcome.friction->Value(), model.friction, 1e-4f);
    EXPECT_NEAR(outcome.inertia->Value(), model.inertia, 1e-4f);
    EXPECT_FALSE(identification->IsRunning());
}

TEST_F(MechanicalParametersIdentificationTest, a_convergence_scheduled_before_an_abort_does_not_complete_the_run_started_after_it)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::seconds{ 60 }
    };
    MotorModel model;
    Excitation excitation{ model };

    bool firstFired = false;
    bool secondFired = false;

    ExpectEncoderToFollow(excitation);
    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));
    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ model.torqueConstant }, model.polePairs, config, [&firstFired](auto, auto)
        {
            firstFired = true;
        });
    PublishExcitation(excitation, samplesUntilConvergence);

    ExpectDriveReleased();
    identification->Abort();

    ExpectRunStarted();
    EXPECT_CALL(controllerMock, CommandSpeed(_));
    identification->EstimateFrictionAndInertia(foc::NewtonMeter{ model.torqueConstant }, model.polePairs, config, [&secondFired](auto, auto)
        {
            secondFired = true;
        });

    ExecuteAllActions();

    EXPECT_FALSE(firstFired);
    EXPECT_FALSE(secondFired);
    EXPECT_TRUE(identification->IsRunning());

    ExpectDriveReleased();
    identification->Abort();
}
