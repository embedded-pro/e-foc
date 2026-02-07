#include "source/foc/implementations/TorqueControllerImpl.hpp"
#include "source/foc/implementations/test_doubles/DriversMock.hpp"
#include "source/foc/implementations/test_doubles/FieldOrientedControllerMock.hpp"
#include "source/foc/interfaces/FieldOrientedController.hpp"
#include "gmock/gmock.h"

namespace
{
    using namespace testing;

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

    class TestTorqueController
        : public ::testing::Test
    {
    public:
        TestTorqueController()
        {
            EXPECT_CALL(interfaceMock, BaseFrequency())
                .WillOnce(::testing::Return(hal::Hertz{ 10000 }));
            EXPECT_CALL(interfaceMock, PhaseCurrentsReady(::testing::_, ::testing::_))
                .WillOnce([this](auto, const auto& onDone)
                    {
                        interfaceMock.StorePhaseCurrentsCallback(onDone);
                    });

            foc.emplace(interfaceMock, encoderMock, focMock);
        }

        ::testing::StrictMock<foc::FieldOrientedControllerInterfaceMock> interfaceMock;
        ::testing::StrictMock<foc::EncoderMock> encoderMock;
        ::testing::StrictMock<foc::FieldOrientedControllerMock> focMock;
        std::optional<foc::TorqueControllerImpl> foc;
    };
}

TEST_F(TestTorqueController, enable_starts_interface_and_enables_pid_controllers)
{
    EXPECT_CALL(focMock, Reset());
    EXPECT_CALL(interfaceMock, Start());
    foc->Enable();

    EXPECT_TRUE(foc->IsRunning());
}

TEST_F(TestTorqueController, disable_stops_interface_and_disables_pid_controllers)
{
    EXPECT_CALL(focMock, Reset());
    EXPECT_CALL(interfaceMock, Start());
    foc->Enable();

    EXPECT_CALL(interfaceMock, Stop());
    foc->Disable();

    EXPECT_FALSE(foc->IsRunning());
}

TEST_F(TestTorqueController, set_tunings_updates_pid_controllers)
{
    foc::Volts Vdc{ 1.0f };
    controllers::PidTunings<float> dTunings{ 1.0f, 0.5f, 0.1f };
    controllers::PidTunings<float> qTunings{ 2.0f, 1.0f, 0.2f };

    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsEq(std::make_pair(dTunings, qTunings))));
    foc->SetCurrentTunings(Vdc, { dTunings, qTunings });
}

TEST_F(TestTorqueController, set_point_updates_pid_controllers)
{
    foc::Ampere dSetpoint{ 0.5f };
    foc::Ampere qSetpoint{ 0.75f };

    EXPECT_CALL(focMock, SetPoint(::testing::Pair(dSetpoint, qSetpoint)));
    foc->SetPoint({ dSetpoint, qSetpoint });
}

TEST_F(TestTorqueController, phase_currents_callback_triggers_foc_calculation_and_output)
{
    foc::PhaseCurrents currentPhases{ foc::Ampere{ 100 }, foc::Ampere{ 200 }, foc::Ampere{ 300 } };
    foc::PhasePwmDutyCycles pwmOutput{ hal::Percent{ 25 }, hal::Percent{ 50 }, hal::Percent{ 75 } };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(PhaseCurrentsEq(currentPhases), ::testing::_))
        .WillOnce(::testing::Return(pwmOutput));
    EXPECT_CALL(interfaceMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(pwmOutput))).Times(1);

    interfaceMock.TriggerPhaseCurrentsCallback(currentPhases);
}

TEST_F(TestTorqueController, disabled_pid_controllers_are_reenabled_after_enable)
{
    EXPECT_CALL(interfaceMock, Stop());
    foc->Disable();
    EXPECT_FALSE(foc->IsRunning());

    EXPECT_CALL(focMock, Reset());
    EXPECT_CALL(interfaceMock, Start());
    foc->Enable();
    EXPECT_TRUE(foc->IsRunning());

    foc::PhaseCurrents currentPhases{ foc::Ampere{ 100 }, foc::Ampere{ 200 }, foc::Ampere{ 300 } };
    foc::PhasePwmDutyCycles pwmOutput{ hal::Percent{ 30 }, hal::Percent{ 60 }, hal::Percent{ 90 } };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(PhaseCurrentsEq(currentPhases), ::testing::_))
        .WillOnce(::testing::Return(pwmOutput));
    EXPECT_CALL(interfaceMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(pwmOutput))).Times(1);

    interfaceMock.TriggerPhaseCurrentsCallback(currentPhases);
}

TEST_F(TestTorqueController, phase_currents_with_modified_pid_values)
{
    foc::Volts Vdc{ 1.0f };
    controllers::PidTunings<float> dTunings{ 1.0f, 0.5f, 0.1f };
    controllers::PidTunings<float> qTunings{ 2.0f, 1.0f, 0.2f };
    EXPECT_CALL(focMock, SetCurrentTunings(::testing::Eq(Vdc), IdAndIqTuningsEq(std::make_pair(dTunings, qTunings))));
    foc->SetCurrentTunings(Vdc, { dTunings, qTunings });

    foc::Ampere dSetpoint{ 0.5f };
    foc::Ampere qSetpoint{ 0.75f };
    EXPECT_CALL(focMock, SetPoint(::testing::Pair(dSetpoint, qSetpoint)));
    foc->SetPoint({ dSetpoint, qSetpoint });

    foc::PhaseCurrents currentPhases{ foc::Ampere{ 150 }, foc::Ampere{ 250 }, foc::Ampere{ 350 } };
    foc::PhasePwmDutyCycles pwmOutput{ hal::Percent{ 40 }, hal::Percent{ 60 }, hal::Percent{ 80 } };

    EXPECT_CALL(encoderMock, Read())
        .WillOnce(::testing::Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(focMock, Calculate(PhaseCurrentsEq(currentPhases), ::testing::_))
        .WillOnce(::testing::Return(pwmOutput));
    EXPECT_CALL(interfaceMock, ThreePhasePwmOutput(PhasePwmDutyCyclesEq(pwmOutput))).Times(1);

    interfaceMock.TriggerPhaseCurrentsCallback(currentPhases);
}
