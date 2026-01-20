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
    services::MechanicalParametersIdentification::Config config{
        foc::RadiansPerSecond{ 52.36f },
        0.998f,
        std::chrono::seconds{ 1 }
    };

    EXPECT_CALL(encoderMock, Read()).WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(controllerMock, Enable());
    EXPECT_CALL(controllerMock, SetPoint(foc::RadiansPerSecond{ 52.36f }));
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
    EXPECT_CALL(controllerMock, Enable());
    EXPECT_CALL(controllerMock, SetPoint(::testing::_));
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
    EXPECT_CALL(controllerMock, Enable());
    EXPECT_CALL(controllerMock, SetPoint(::testing::_));
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
