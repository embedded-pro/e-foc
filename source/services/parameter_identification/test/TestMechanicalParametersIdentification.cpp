#include "infra/timer/test_helper/ClockFixture.hpp"
#include "source/foc/implementations/test_doubles/ControllerMock.hpp"
#include "source/foc/implementations/test_doubles/DriversMock.hpp"
#include "source/services/parameter_identification/MechanicalParametersIdentificationImpl.hpp"
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
        StrictMock<foc::SpeedControllerMock> controllerMock;
        StrictMock<foc::FieldOrientedControllerInterfaceMock> driverMock;
        StrictMock<foc::EncoderMock> encoderMock;
        services::MechanicalParametersIdentificationImpl identification{ controllerMock, driverMock, encoderMock };

        infra::Function<void(foc::PhaseCurrents)> phaseCurrentsCallback;
    };
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_enables_controller_and_sets_target_speed)
{
    services::MechanicalParametersIdentification::FrictionConfig config{
        foc::RadiansPerSecond{ 52.36f },
        std::chrono::seconds{ 1 },
        std::chrono::seconds{ 1 },
        foc::NewtonMeter{ 0.1f }
    };

    EXPECT_CALL(controllerMock, Enable());
    EXPECT_CALL(controllerMock, SetPoint(foc::RadiansPerSecond{ 52.36f }));

    identification.EstimateFriction(config, [](auto) {});
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_waits_for_settle_time_before_sampling)
{
    services::MechanicalParametersIdentification::FrictionConfig config{
        foc::RadiansPerSecond{ 50.0f },
        std::chrono::milliseconds{ 100 },
        std::chrono::milliseconds{ 50 },
        foc::NewtonMeter{ 0.1f }
    };

    EXPECT_CALL(controllerMock, Enable());
    EXPECT_CALL(controllerMock, SetPoint(::testing::_));

    identification.EstimateFriction(config, [](auto) {});

    ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .WillOnce(::testing::SaveArg<1>(&phaseCurrentsCallback));

    ForwardTime(std::chrono::milliseconds{ 50 });
}

TEST_F(MechanicalParametersIdentificationTest, estimate_friction_calculates_damping_from_steady_state_current)
{
    services::MechanicalParametersIdentification::FrictionConfig config{
        foc::RadiansPerSecond{ 50.0f },
        std::chrono::milliseconds{ 100 },
        std::chrono::seconds{ 1 },
        foc::NewtonMeter{ 0.1f }
    };

    std::optional<foc::NewtonMeterSecondPerRadian> resultDamping;

    EXPECT_CALL(controllerMock, Enable());
    EXPECT_CALL(controllerMock, SetPoint(::testing::_));

    identification.EstimateFriction(config, [&](auto damping)
        {
            resultDamping = damping;
        });

    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .WillOnce(::testing::SaveArg<1>(&phaseCurrentsCallback));

    ForwardTime(std::chrono::milliseconds{ 100 });

    float steadyStateCurrent = 5.0f;
    float Kt = 0.1f;
    float targetSpeed = 50.0f;
    float expectedTorque = steadyStateCurrent * Kt;
    float expectedDamping = expectedTorque / targetSpeed;

    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_));
    EXPECT_CALL(controllerMock, Disable());
    EXPECT_CALL(driverMock, Stop());

    for (std::size_t i = 0; i < 200 + 15; ++i)
        phaseCurrentsCallback(foc::PhaseCurrents{
            foc::Ampere{ 3.0f },
            foc::Ampere{ 4.0f },
            foc::Ampere{ steadyStateCurrent } });

    ASSERT_TRUE(resultDamping.has_value());
    EXPECT_NEAR(resultDamping->Value(), expectedDamping, 0.001f);
}

TEST_F(MechanicalParametersIdentificationTest, estimate_inertia_disables_controller_initially)
{
    services::MechanicalParametersIdentification::InertiaConfig config{
        foc::Ampere{ 1.0f },
        std::chrono::milliseconds{ 500 },
        foc::NewtonMeter{ 0.1f }
    };

    foc::NewtonMeterSecondPerRadian damping{ 0.05f };

    EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(controllerMock, Disable());
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateInertia(config, damping, [](auto) {});
}

TEST_F(MechanicalParametersIdentificationTest, estimate_inertia_collects_speed_samples)
{
    services::MechanicalParametersIdentification::InertiaConfig config{
        foc::Ampere{ 2.0f },
        std::chrono::milliseconds{ 500 },
        foc::NewtonMeter{ 0.15f }
    };

    foc::NewtonMeterSecondPerRadian damping{ 0.05f };

    EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(controllerMock, Disable());
    EXPECT_CALL(driverMock, Stop());

    identification.EstimateInertia(config, damping, [](auto) {});

    EXPECT_CALL(controllerMock, Enable());
    EXPECT_CALL(controllerMock, SetPoint(foc::RadiansPerSecond{ 0.0f }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_))
        .WillOnce(::testing::SaveArg<1>(&phaseCurrentsCallback));

    ForwardTime(std::chrono::milliseconds{ 500 });

    EXPECT_CALL(encoderMock, Read())
        .WillRepeatedly(::testing::Return(foc::Radians{ 0.01f }));
    EXPECT_CALL(driverMock, PhaseCurrentsReady(::testing::_, ::testing::_));
    EXPECT_CALL(controllerMock, Disable());
    EXPECT_CALL(driverMock, Stop());

    for (std::size_t i = 0; i < 500; ++i)
        phaseCurrentsCallback(foc::PhaseCurrents{});
}
