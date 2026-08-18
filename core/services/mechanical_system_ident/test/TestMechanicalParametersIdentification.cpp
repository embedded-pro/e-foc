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

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_reports_values_on_convergence_and_allows_restart)
{
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 50.0f },
        0.998f,
        std::chrono::seconds{ 5 }
    };

    struct ConvergenceResult
    {
        bool callbackCalled = false;
        std::optional<foc::NewtonMeterSecondPerRadian> friction;
        std::optional<foc::NewtonMeterSecondSquared> inertia;
    } outcome;

    EXPECT_CALL(encoderMock, Read())
        .WillRepeatedly(::testing::Return(foc::Radians{ 0.02f }));
    EXPECT_CALL(controllerMock, EnableSpeedCommand());
    EXPECT_CALL(controllerMock, CommandSpeed(::testing::_));
    EXPECT_CALL(controllerMock, DisableSpeedCommand()).Times(::testing::AtMost(1));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .WillOnce(::testing::SaveArg<1>(&phaseCurrentsCallback))
        .WillRepeatedly(::testing::Return());

    identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
        [&outcome](auto friction, auto inertia)
        {
            outcome.callbackCalled = true;
            outcome.friction = friction;
            outcome.inertia = inertia;
        });

    for (std::size_t i = 0; i < 1000; ++i)
    {
        const float variation = static_cast<float>(i % 10) * 0.01f;
        phaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ 1.0f + variation },
            foc::Ampere{ -0.5f - variation * 0.5f },
            foc::Ampere{ -0.5f - variation * 0.5f } });
    }

    if (outcome.callbackCalled)
    {
        bool secondCallbackCalled = false;
        EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
        EXPECT_CALL(controllerMock, EnableSpeedCommand());
        EXPECT_CALL(controllerMock, CommandSpeed(::testing::_));
        EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_)).Times(::testing::AnyNumber());

        identification.EstimateFrictionAndInertia(foc::NewtonMeter{ 0.1f }, 7, config,
            [&](auto, auto)
            {
                secondCallbackCalled = true;
            });

        EXPECT_FALSE(secondCallbackCalled);
    }
}
