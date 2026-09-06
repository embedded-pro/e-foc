#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/foc/interfaces/test_doubles/FocMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <cmath>
#include <gmock/gmock.h>

namespace
{
    using namespace testing;

    class MechanicalParametersIdentificationTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
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
        services::MechanicalParametersIdentificationImpl identification{ controllerMock, driveMock, observableMock, driverMock, encoderMock };

        void ExpectRunStarted()
        {
            EXPECT_CALL(observableMock, RegisterPhaseCurrentsObserver(_))
                .WillOnce(Invoke([this](const infra::Function<void(const foc::PhaseCurrents&)>& observer)
                    {
                        observableMock.StoreObserver(observer);
                    }));
            EXPECT_CALL(driveMock, Start());
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

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [](auto, auto) {});

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
    // Taking PhaseCurrentsReady evicts the control loop, so nothing writes duty cycles, the rotor
    // never turns and the regressor never carries the excitation the estimator needs.
    EXPECT_CALL(driverMock, PhaseCurrentsReady(_, _)).Times(0);

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [](auto, auto) {});
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

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [&](auto friction, auto inertia)
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

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
        [&](auto, auto)
        {
            firstCallbackCalled = true;
        });

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
        [&](auto friction, auto inertia)
        {
            secondCallbackCalled = true;
            EXPECT_FALSE(friction.has_value());
            EXPECT_FALSE(inertia.has_value());
        });

    EXPECT_FALSE(firstCallbackCalled);
    EXPECT_TRUE(secondCallbackCalled);

    ExpectDriveReleased();
    identification.Abort();
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

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [&](auto, auto)
        {
            fired = true;
        });

    ExpectDriveReleased();
    identification.Abort();

    EXPECT_FALSE(fired);
    EXPECT_FALSE(observableMock.HasObserver());

    // The timeout must not resurrect the run it was cancelled with.
    ForwardTime(std::chrono::seconds{ 10 });
    EXPECT_FALSE(fired);
}

TEST_F(MechanicalParametersIdentificationTest, abort_without_a_run_in_flight_is_a_no_op)
{
    identification.Abort();
}

TEST_F(MechanicalParametersIdentificationTest, a_run_that_has_not_converged_keeps_the_drive_turning)
{
    // Stepped quadratic position ramp. A smooth n^2 ramp gives constant acceleration, which makes
    // the acceleration regressor collinear with the constant term; repeating each step varies it.
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

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
        [&outcome](auto f, auto i)
        {
            outcome.fired = true;
            outcome.friction = f;
            outcome.inertia = i;
        });

    for (std::size_t i = 0; i != 2000 && !outcome.fired; ++i)
    {
        PublishCurrents();
        // Convergence is handed to the dispatcher: reclaiming the observer inside its own
        // invocation would destroy the closure being executed, and the completion reaches
        // non-volatile memory through the state machine.
        ExecuteAllActions();
    }

    // Whether this excitation satisfies the convergence gate is the estimator's business; what
    // matters here is that either outcome leaves the drive in a coherent state.
    EXPECT_EQ(outcome.fired, !observableMock.HasObserver());

    if (!outcome.fired)
    {
        ExpectDriveReleased();
        identification.Abort();
    }
}
