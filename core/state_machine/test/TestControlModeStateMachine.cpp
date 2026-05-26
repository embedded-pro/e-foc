#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/foc/implementations/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <gtest/gtest.h>

namespace
{
    using namespace testing;

    using TestedControlMode = state_machine::ControlModeStateMachine<
        foc::FocTorqueImpl,
        foc::FocSpeedImpl,
        foc::FocPositionImpl>;

    class ControlModeStateMachineTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        hal::SerialCommunicationMock communication;
        infra::Execute setupStreamExpectations{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(_, _)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Available()).Times(AnyNumber()).WillRepeatedly(Return(1000));
                EXPECT_CALL(streamWriterMock, ConstructSaveMarker()).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, GetProcessedBytesSince(_)).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, SaveState(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(streamWriterMock, RestoreState(_)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Overwrite(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(communication, SendDataMock(_)).Times(AnyNumber());
            } };
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ communication, tracer };
        services::TerminalWithStorage::WithMaxSize<20> terminal{ terminalWithCommands, tracer };

        StrictMock<foc::FieldOrientedControllerInterfaceMock> inverterMock;
        StrictMock<foc::EncoderMock> encoderMock;
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<services::MechanicalParametersIdentificationMock> mechIdentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;

        infra::Execute setupHardwareExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Register(_))
                    .Times(AnyNumber())
                    .WillRepeatedly(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
                        {
                            faultNotifierMock.StoreHandler(handler);
                        }));
            } };

        std::optional<TestedControlMode> subject;

        void GivenNvmAlwaysInvalid()
        {
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](infra::Function<void(bool)> onDone)
                    {
                        onDone(false);
                    }));
        }

        void GivenNvmSaveConfigSucceeds()
        {
            EXPECT_CALL(nvmMock, SaveConfig(_, _))
                .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        onDone(services::NvmStatus::Ok);
                    }));
        }

        void GivenNvmSaveConfigFails()
        {
            EXPECT_CALL(nvmMock, SaveConfig(_, _))
                .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        onDone(services::NvmStatus::WriteFailed);
                    }));
        }

        void ConstructSubject(uint8_t defaultMode = 0)
        {
            services::ConfigData config{};
            config.defaultControlMode = defaultMode;
            subject.emplace(
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
                faultNotifierMock,
                config,
                TestedControlMode::OuterLoopArgs{
                    foc::Ampere{ 10.0f },
                    hal::Hertz{ 1000 },
                    lowPriorityInterruptMock });
        }
    };

    // ---- Active() initial state ----

    TEST_F(ControlModeStateMachineTest, Active_Returns_Torque_After_Default_Construction)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject(/*defaultMode=*/0);

        EXPECT_EQ(subject->Active(), state_machine::ControlMode::torque);
    }

    TEST_F(ControlModeStateMachineTest, Active_Returns_Speed_When_Constructed_With_Speed_Default)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject(/*defaultMode=*/static_cast<uint8_t>(state_machine::ControlMode::speed));

        EXPECT_EQ(subject->Active(), state_machine::ControlMode::speed);
    }

    // ---- TrySet* returns false when wrong mode active ----

    TEST_F(ControlModeStateMachineTest, TrySetSpeed_Returns_False_When_Torque_Active)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject();

        EXPECT_FALSE(subject->TrySetSpeed(foc::RadiansPerSecond{ 10.0f }));
    }

    TEST_F(ControlModeStateMachineTest, TrySetPosition_Returns_False_When_Torque_Active)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject();

        EXPECT_FALSE(subject->TrySetPosition(foc::Radians{ 1.0f }));
    }

    TEST_F(ControlModeStateMachineTest, TrySetTorque_Returns_False_When_Speed_Active)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();

        subject->Select(state_machine::ControlMode::speed, [](auto) {});

        EXPECT_FALSE(subject->TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } }));
    }

    // ---- Select() NVM success ----

    TEST_F(ControlModeStateMachineTest, Select_Switches_Active_To_Speed_On_Nvm_Ok)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();

        state_machine::SelectResult result{ state_machine::SelectResult::nvmFailed };
        subject->Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
            {
                result = r;
            });

        EXPECT_EQ(result, state_machine::SelectResult::ok);
        EXPECT_EQ(subject->Active(), state_machine::ControlMode::speed);
    }

    TEST_F(ControlModeStateMachineTest, Select_Switches_Active_To_Position_On_Nvm_Ok)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();

        state_machine::SelectResult result{ state_machine::SelectResult::nvmFailed };
        subject->Select(state_machine::ControlMode::position, [&result](state_machine::SelectResult r)
            {
                result = r;
            });

        EXPECT_EQ(result, state_machine::SelectResult::ok);
        EXPECT_EQ(subject->Active(), state_machine::ControlMode::position);
    }

    // ---- C3: NVM failure rollback ----

    TEST_F(ControlModeStateMachineTest, Select_RollsBack_Mode_On_Nvm_Failure)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigFails();
        ConstructSubject();

        state_machine::SelectResult result{ state_machine::SelectResult::ok };
        subject->Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
            {
                result = r;
            });

        EXPECT_EQ(result, state_machine::SelectResult::nvmFailed);
        EXPECT_EQ(subject->Active(), state_machine::ControlMode::torque);
    }

    // ---- C2: In-flight select guard ----

    TEST_F(ControlModeStateMachineTest, Select_While_Previous_Select_Pending_Returns_Busy)
    {
        GivenNvmAlwaysInvalid();

        infra::Function<void(services::NvmStatus)> capturedNvmCallback;
        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([&capturedNvmCallback](const services::ConfigData&, infra::Function<void(services::NvmStatus)> cb)
                {
                    capturedNvmCallback = cb;
                }));

        ConstructSubject();

        state_machine::SelectResult firstResult{ state_machine::SelectResult::ok };
        subject->Select(state_machine::ControlMode::speed, [&firstResult](state_machine::SelectResult r)
            {
                firstResult = r;
            });

        state_machine::SelectResult secondResult{ state_machine::SelectResult::ok };
        subject->Select(state_machine::ControlMode::speed, [&secondResult](state_machine::SelectResult r)
            {
                secondResult = r;
            });

        EXPECT_EQ(secondResult, state_machine::SelectResult::busy);

        capturedNvmCallback(services::NvmStatus::WriteFailed);
        EXPECT_EQ(firstResult, state_machine::SelectResult::nvmFailed);
    }

    // ---- Additional helpers ----

    class ControlModeStateMachineExtTest
        : public ControlModeStateMachineTest
    {
    public:
        void GivenNvmValid()
        {
            services::CalibrationData data{};
            data.polePairs = 7;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.lQ = 1.0f;
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
                    {
                        onDone(true);
                    }));
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([data](services::CalibrationData& out,
                                     infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = data;
                        onDone(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
        }

        void InvokeCliCommand(const char* shortName)
        {
            std::string cmd{ shortName };
            cmd += '\r';
            communication.dataReceived(infra::MakeStringByteRange(cmd));
            ExecuteAllActions();
        }
    };

    // ---- Active() — position branch ----

    TEST_F(ControlModeStateMachineExtTest, Active_Returns_Position_When_Constructed_With_Position_Default)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject(/*defaultMode=*/static_cast<uint8_t>(state_machine::ControlMode::position));

        EXPECT_EQ(subject->Active(), state_machine::ControlMode::position);
    }

    // ---- TrySet* returns true when correct mode active ----

    TEST_F(ControlModeStateMachineExtTest, TrySetTorque_Returns_True_When_Torque_Mode_Active)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject();

        EXPECT_TRUE(subject->TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } }));
    }

    TEST_F(ControlModeStateMachineExtTest, TrySetSpeed_Returns_True_When_Speed_Mode_Active)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();

        subject->Select(state_machine::ControlMode::speed, [](auto) {});

        EXPECT_TRUE(subject->TrySetSpeed(foc::RadiansPerSecond{ 10.0f }));
    }

    TEST_F(ControlModeStateMachineExtTest, TrySetPosition_Returns_True_When_Position_Mode_Active)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();

        subject->Select(state_machine::ControlMode::position, [](auto) {});

        EXPECT_TRUE(subject->TrySetPosition(foc::Radians{ 1.0f }));
    }

    // ---- TrySetTorque returns false when position active ----

    TEST_F(ControlModeStateMachineExtTest, TrySetTorque_Returns_False_When_Position_Active)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();

        subject->Select(state_machine::ControlMode::position, [](auto) {});

        EXPECT_FALSE(subject->TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } }));
    }

    // ---- ActiveStateMachine() in each mode ----

    TEST_F(ControlModeStateMachineExtTest, ActiveStateMachine_Is_Accessible_In_Speed_Mode)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();

        subject->Select(state_machine::ControlMode::speed, [](auto) {});

        // CmdDisable is a no-op in Idle — just verifies no crash and correct SM is returned
        subject->ActiveStateMachine().CmdDisable();
    }

    TEST_F(ControlModeStateMachineExtTest, ActiveStateMachine_Is_Accessible_In_Position_Mode)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();

        subject->Select(state_machine::ControlMode::position, [](auto) {});

        subject->ActiveStateMachine().CmdDisable();
    }

    // ---- Select() returns busy when motor is enabled ----

    TEST_F(ControlModeStateMachineExtTest, Select_Returns_Busy_When_Motor_Is_Enabled)
    {
        GivenNvmValid();
        ConstructSubject();

        EXPECT_CALL(inverterMock, Start()).Times(1);
        subject->ActiveStateMachine().CmdEnable();

        state_machine::SelectResult result{ state_machine::SelectResult::ok };
        subject->Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
            {
                result = r;
            });

        EXPECT_EQ(result, state_machine::SelectResult::busy);
    }

    // ---- CLI commands: no-ops in Idle ----

    TEST_F(ControlModeStateMachineExtTest, Cli_Enable_Is_NoOp_In_Idle)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject();

        InvokeCliCommand("en");

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            subject->ActiveStateMachine().CurrentState()));
    }

    TEST_F(ControlModeStateMachineExtTest, Cli_Disable_Is_NoOp_In_Idle)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject();

        InvokeCliCommand("dis");

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            subject->ActiveStateMachine().CurrentState()));
    }

    TEST_F(ControlModeStateMachineExtTest, Cli_ClearFault_Is_NoOp_In_Idle)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject();

        InvokeCliCommand("cf");

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            subject->ActiveStateMachine().CurrentState()));
    }

    // ---- CLI: calibrate starts calibration sequence ----

    TEST_F(ControlModeStateMachineExtTest, Cli_Calibrate_In_Idle_Starts_Calibration)
    {
        GivenNvmAlwaysInvalid();
        infra::Function<void(std::optional<std::size_t>)> capturedCb;
        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
            .WillOnce(Invoke([&capturedCb](const auto&,
                                 const infra::Function<void(std::optional<std::size_t>)>& cb)
                {
                    capturedCb = cb;
                }));
        ConstructSubject();

        InvokeCliCommand("cal");

        EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(
            subject->ActiveStateMachine().CurrentState()));
    }

    // ---- CLI: clear_cal from Idle invalidates NVM ----

    TEST_F(ControlModeStateMachineExtTest, Cli_ClearCal_In_Idle_Calls_InvalidateCalibration)
    {
        GivenNvmAlwaysInvalid();
        EXPECT_CALL(nvmMock, InvalidateCalibration(_))
            .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
                {
                    onDone(services::NvmStatus::Ok);
                }));
        ConstructSubject();

        InvokeCliCommand("cc");

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            subject->ActiveStateMachine().CurrentState()));
    }

    // ---- CLI: apply_estimates ----

    TEST_F(ControlModeStateMachineExtTest, Cli_ApplyEstimates_Is_NoOp_In_Torque_Mode)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject();

        // No mock expectations — guard prevents ApplyOnlineEstimates() from being called
        InvokeCliCommand("ae");
    }

    TEST_F(ControlModeStateMachineExtTest, Cli_ApplyEstimates_In_Speed_Mode_Does_Not_Crash)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();
        subject->Select(state_machine::ControlMode::speed, [](auto) {});

        // ApplyOnlineEstimates() returns early when SM is not Enabled — no mock expectations
        InvokeCliCommand("ae");
    }

    // ---- CLI: active_mode prints each mode ----

    TEST_F(ControlModeStateMachineExtTest, Cli_ActiveMode_Prints_Torque)
    {
        GivenNvmAlwaysInvalid();
        ConstructSubject();

        InvokeCliCommand("am");
    }

    TEST_F(ControlModeStateMachineExtTest, Cli_ActiveMode_Prints_Speed)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();
        subject->Select(state_machine::ControlMode::speed, [](auto) {});

        InvokeCliCommand("am");
    }

    TEST_F(ControlModeStateMachineExtTest, Cli_ActiveMode_Prints_Position)
    {
        GivenNvmAlwaysInvalid();
        GivenNvmSaveConfigSucceeds();
        ConstructSubject();
        subject->Select(state_machine::ControlMode::position, [](auto) {});

        InvokeCliCommand("am");
    }
}
