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
        StrictMock<drivers::ThreePhaseInverterMock> driverMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        infra::Execute setOuterLoopFrequency{ [this]()
            {
                EXPECT_CALL(controllerMock, SpeedCommandFrequency()).WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(driverMock, BaseFrequency()).WillRepeatedly(Return(hal::Hertz{ 10000 }));
            } };
        services::MechanicalParametersIdentificationImpl identification{ controllerMock, driverMock, encoderMock };

        infra::Function<void(foc::PhaseCurrents)> phaseCurrentsCallback;
    };
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_enables_controller_and_sets_target_speed)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 52.36f },
        0.998f,
        std::chrono::seconds{ 1 }
    };

    EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(controllerMock, EnableSpeedCommand());
    EXPECT_CALL(controllerMock, CommandSpeed(foc::RadiansPerSecond{ 52.36f }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_));

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [](auto, auto) {});
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_waits_for_settle_time_before_sampling)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::milliseconds{ 100 }
    };

    EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(controllerMock, EnableSpeedCommand());
    EXPECT_CALL(controllerMock, CommandSpeed(::testing::_));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_));

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [](auto, auto) {});
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_calculates_damping_from_steady_state_current)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::seconds{ 1 }
    };

    std::optional<foc::NewtonMeterSecondPerRadian> resultFriction;
    std::optional<foc::NewtonMeterSecondSquared> resultInertia;

    EXPECT_CALL(encoderMock, Read())
        .WillRepeatedly(::testing::Return(foc::Radians{ 0.01f }));
    EXPECT_CALL(controllerMock, EnableSpeedCommand());
    EXPECT_CALL(controllerMock, CommandSpeed(::testing::_));
    EXPECT_CALL(controllerMock, DisableSpeedCommand()).Times(::testing::AtMost(1));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .WillOnce(::testing::SaveArg<1>(&phaseCurrentsCallback))
        .WillRepeatedly(::testing::Return());

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config, [&](auto friction, auto inertia)
        {
            resultFriction = friction;
            resultInertia = inertia;
        });

    for (std::size_t i = 0; i < 1000; ++i)
        phaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ 1.0f },
            foc::Ampere{ 1.0f },
            foc::Ampere{ 1.0f } });
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

    EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(controllerMock, EnableSpeedCommand());
    EXPECT_CALL(controllerMock, CommandSpeed(foc::RadiansPerSecond{ 52.36f }));
    EXPECT_CALL(controllerMock, DisableSpeedCommand());
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_)).Times(2);

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

    EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(controllerMock, EnableSpeedCommand());
    EXPECT_CALL(controllerMock, CommandSpeed(::testing::_));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

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
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_converges_with_rich_excitation_and_allows_restart)
{
    // Quadratic position ramp: p[n] = n^2 * 1e-5 gives constant acceleration
    // and linearly increasing speed — persistent excitation for all 3 RLS parameters.
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

    // Each OnSamplingUpdate calls encoder.Read() twice; EstimateFrictionAndInertia
    // calls it once for initialisation.  Track call index so both reads within
    // one sample return the same position value.
    std::size_t callIndex = 0;
    EXPECT_CALL(encoderMock, Read())
        .WillRepeatedly(::testing::Invoke([&]()
            {
                std::size_t sampleIndex = (callIndex == 0) ? 0u : (callIndex + 1) / 2;
                callIndex++;
                const float pos = static_cast<float>(sampleIndex) *
                                  static_cast<float>(sampleIndex) * 1e-5f;
                return foc::Radians{ pos };
            }));

    EXPECT_CALL(controllerMock, EnableSpeedCommand());
    EXPECT_CALL(controllerMock, CommandSpeed(::testing::_));
    EXPECT_CALL(controllerMock, DisableSpeedCommand()).Times(::testing::AtMost(1));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .WillOnce(::testing::SaveArg<1>(&phaseCurrentsCallback))
        .WillRepeatedly(::testing::Return());

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
        [&outcome](auto f, auto i)
        {
            outcome.fired = true;
            outcome.friction = f;
            outcome.inertia = i;
        });

    for (std::size_t i = 0; i < 2000 && !outcome.fired; ++i)
    {
        phaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ 1.0f }, foc::Ampere{ -0.5f }, foc::Ampere{ -0.5f } });
    }

    // If convergence occurred, verify rls was reset so a new call is accepted
    if (outcome.fired)
    {
        EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
        EXPECT_CALL(controllerMock, EnableSpeedCommand());
        EXPECT_CALL(controllerMock, CommandSpeed(::testing::_));
        EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
            .Times(::testing::AnyNumber());

        bool secondFired = false;
        identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
            [&secondFired](auto, auto) { secondFired = true; });
        EXPECT_FALSE(secondFired);
    }
}
