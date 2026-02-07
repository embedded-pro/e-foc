#include "source/foc/implementations/SpeedControllerImpl.hpp"
#include "source/foc/implementations/test_doubles/DriversMock.hpp"
#include "source/foc/implementations/test_doubles/FieldOrientedControllerMock.hpp"
#include "source/foc/interfaces/FieldOrientedController.hpp"
#include "gmock/gmock.h"

namespace
{
    using namespace testing;

    MATCHER_P(SpeedTuningsEq, expected, "")
    {
        return std::abs(arg.kp - expected.kp) < 1e-6f &&
               std::abs(arg.ki - expected.ki) < 1e-6f &&
               std::abs(arg.kd - expected.kd) < 1e-6f;
    }

    MATCHER_P(IdAndIqTuningsEq, expected, "")
    {
        return std::abs(arg.first.kp - expected.first.kp) < 1e-6f &&
               std::abs(arg.first.ki - expected.first.ki) < 1e-6f &&
               std::abs(arg.first.kd - expected.first.kd) < 1e-6f &&
               std::abs(arg.second.kp - expected.second.kp) < 1e-6f &&
               std::abs(arg.second.ki - expected.second.ki) < 1e-6f &&
               std::abs(arg.second.kd - expected.second.kd) < 1e-6f;
    }

    MATCHER_P(PhaseCurrentsEq, expected, "")
    {
        return std::abs(arg.a.Value() - expected.a.Value()) < 1e-6f &&
               std::abs(arg.b.Value() - expected.b.Value()) < 1e-6f &&
               std::abs(arg.c.Value() - expected.c.Value()) < 1e-6f;
    }

    MATCHER_P(PhasePwmDutyCyclesEq, expected, "")
    {
        return std::abs(arg.a.Value() - expected.a.Value()) < 1e-6f &&
               std::abs(arg.b.Value() - expected.b.Value()) < 1e-6f &&
               std::abs(arg.c.Value() - expected.c.Value()) < 1e-6f;
    }

    class TestSpeedController
        : public ::testing::Test
    {
    public:
        TestSpeedController()
        {
            EXPECT_CALL(interfaceMock, BaseFrequency())
                .WillOnce(::testing::Return(hal::Hertz{ 10000 }));
            EXPECT_CALL(interfaceMock, PhaseCurrentsReady(::testing::_, ::testing::_))
                .WillOnce([this](auto, const auto& onDone)
                    {
                        interfaceMock.StorePhaseCurrentsCallback(onDone);
                    });

            controller.emplace(interfaceMock, encoderMock, focMock);
        }

        ::testing::StrictMock<foc::FieldOrientedControllerInterfaceMock> interfaceMock;
        ::testing::StrictMock<foc::EncoderMock> encoderMock;
        ::testing::StrictMock<foc::FieldOrientedControllerSpeedControlMock> focMock;
        std::optional<foc::SpeedControllerImpl> controller;
    };
}

TEST_F(TestSpeedController, enable_starts_interface_and_resets_foc)
{
    EXPECT_CALL(focMock, Reset());
    EXPECT_CALL(interfaceMock, Start());
    controller->Enable();

    EXPECT_TRUE(controller->IsRunning());
}

TEST_F(TestSpeedController, disable_stops_interface)
{
    EXPECT_CALL(focMock, Reset());
    EXPECT_CALL(interfaceMock, Start());
    controller->Enable();

    EXPECT_CALL(interfaceMock, Stop());
    controller->Disable();

    EXPECT_FALSE(controller->IsRunning());
}

TEST_F(TestSpeedController, set_tunings_updates_speed_and_torque_pid_controllers)
{
    foc::Volts Vdc{ 12.0f };
    controllers::PidTunings<float> speedTunings{ 1.0f, 0.5f, 0.1f };
    controllers::PidTunings<float> dTunings{ 2.0f, 1.0f, 0.2f };
    controllers::PidTunings<float> qTunings{ 3.0f, 1.5f, 0.3f };

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsEq(std::make_pair(dTunings, qTunings))));
    EXPECT_CALL(focMock, SetSpeedTunings(::testing::Eq(Vdc), SpeedTuningsEq(speedTunings)));
    controller->SetCurrentTunings(Vdc, { dTunings, qTunings });
    controller->SetSpeedTunings(Vdc, speedTunings);
}

TEST_F(TestSpeedController, set_point_updates_speed_setpoint)
{
    foc::RadiansPerSecond speedSetpoint{ 100.0f };

    EXPECT_CALL(focMock, SetPoint(::testing::Eq(speedSetpoint)));
    controller->SetPoint(speedSetpoint);
}

TEST_F(TestSpeedController, phase_currents_callback_triggers_foc_calculation_and_output)
{
    foc::PhaseCurrents currentPhases{ foc::Ampere{ 1.0f }, foc::Ampere{ 2.0f }, foc::Ampere{ 3.0f } };
    foc::PhasePwmDutyCycles pwmOutput{ hal::Percent{ 25 }, hal::Percent{ 50 }, hal::Percent{ 75 } };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(PhaseCurrentsEq(currentPhases), ::testing::_))
        .WillOnce(::testing::Return(pwmOutput));
    EXPECT_CALL(interfaceMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(pwmOutput))).Times(1);

    interfaceMock.TriggerPhaseCurrentsCallback(currentPhases);
}

TEST_F(TestSpeedController, disabled_controller_stops_processing_and_reenables_after_enable)
{
    EXPECT_CALL(interfaceMock, Stop());
    controller->Disable();
    EXPECT_FALSE(controller->IsRunning());

    EXPECT_CALL(focMock, Reset());
    EXPECT_CALL(interfaceMock, Start());
    controller->Enable();
    EXPECT_TRUE(controller->IsRunning());

    foc::PhaseCurrents currentPhases{ foc::Ampere{ 1.5f }, foc::Ampere{ 2.5f }, foc::Ampere{ 3.5f } };
    foc::PhasePwmDutyCycles pwmOutput{ hal::Percent{ 30 }, hal::Percent{ 60 }, hal::Percent{ 90 } };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(PhaseCurrentsEq(currentPhases), ::testing::_))
        .WillOnce(::testing::Return(pwmOutput));
    EXPECT_CALL(interfaceMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(pwmOutput))).Times(1);

    interfaceMock.TriggerPhaseCurrentsCallback(currentPhases);
}

TEST_F(TestSpeedController, phase_currents_with_modified_speed_and_torque_tunings)
{
    foc::Volts Vdc{ 24.0f };
    controllers::PidTunings<float> speedTunings{ 2.0f, 1.0f, 0.2f };
    controllers::PidTunings<float> dTunings{ 1.5f, 0.75f, 0.15f };
    controllers::PidTunings<float> qTunings{ 2.5f, 1.25f, 0.25f };

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsEq(std::make_pair(dTunings, qTunings))));
    EXPECT_CALL(focMock, SetSpeedTunings(::testing::Eq(Vdc), SpeedTuningsEq(speedTunings)));
    controller->SetCurrentTunings(Vdc, { dTunings, qTunings });
    controller->SetSpeedTunings(Vdc, speedTunings);

    foc::RadiansPerSecond speedSetpoint{ 150.0f };
    EXPECT_CALL(focMock, SetPoint(::testing::Eq(speedSetpoint)));
    controller->SetPoint(speedSetpoint);

    foc::PhaseCurrents currentPhases{ foc::Ampere{ 1.2f }, foc::Ampere{ 2.4f }, foc::Ampere{ 3.6f } };
    foc::PhasePwmDutyCycles pwmOutput{ hal::Percent{ 35 }, hal::Percent{ 55 }, hal::Percent{ 85 } };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(PhaseCurrentsEq(currentPhases), ::testing::_))
        .WillOnce(::testing::Return(pwmOutput));
    EXPECT_CALL(interfaceMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(pwmOutput))).Times(1);

    interfaceMock.TriggerPhaseCurrentsCallback(currentPhases);
}

TEST_F(TestSpeedController, is_not_running_initially)
{
    EXPECT_FALSE(controller->IsRunning());
}

TEST_F(TestSpeedController, multiple_enable_disable_cycles)
{
    EXPECT_CALL(focMock, Reset());
    EXPECT_CALL(interfaceMock, Start());
    controller->Enable();
    EXPECT_TRUE(controller->IsRunning());

    EXPECT_CALL(interfaceMock, Stop());
    controller->Disable();
    EXPECT_FALSE(controller->IsRunning());

    EXPECT_CALL(focMock, Reset());
    EXPECT_CALL(interfaceMock, Start());
    controller->Enable();
    EXPECT_TRUE(controller->IsRunning());

    EXPECT_CALL(interfaceMock, Stop());
    controller->Disable();
    EXPECT_FALSE(controller->IsRunning());
}
