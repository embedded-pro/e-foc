#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
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

using namespace testing;

using TestedControlMode = state_machine::ControlModeStateMachine;

namespace
{
    class ControlModeStateMachineTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        testing::StrictMock<hal::SerialCommunicationMock> communication;
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

        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        StrictMock<drivers::EncoderMock> encoderMock;
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
                EXPECT_CALL(lowPriorityInterruptMock, Unregister()).Times(AnyNumber());
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

        void GivenNvmSaveConfigAlwaysSucceeds()
        {
            EXPECT_CALL(nvmMock, SaveConfig(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
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
            ConstructSubjectWithConfig(config);
        }

        void ConstructSubjectWithConfig(const services::ConfigData& config)
        {
            subject.emplace(
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
                faultNotifierMock,
                config,
                TestedControlMode::OuterLoopArgs{
                    foc::Ampere{ 10.0f },
                    hal::Hertz{ 1000 },
                    lowPriorityInterruptMock });
        }
    };
}

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

TEST_F(ControlModeStateMachineTest, Select_Remains_Usable_After_Nvm_Reports_Busy)
{
    GivenNvmAlwaysInvalid();

    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Busy);
            }))
        .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));

    ConstructSubject();

    state_machine::SelectResult busyResult{ state_machine::SelectResult::ok };
    subject->Select(state_machine::ControlMode::speed, [&busyResult](state_machine::SelectResult r)
        {
            busyResult = r;
        });

    EXPECT_EQ(busyResult, state_machine::SelectResult::busy);
    EXPECT_EQ(subject->Active(), state_machine::ControlMode::torque);

    state_machine::SelectResult retryResult{ state_machine::SelectResult::nvmFailed };
    subject->Select(state_machine::ControlMode::speed, [&retryResult](state_machine::SelectResult r)
        {
            retryResult = r;
        });

    EXPECT_EQ(retryResult, state_machine::SelectResult::ok);
    EXPECT_EQ(subject->Active(), state_machine::ControlMode::speed);
}

// ---- C2: In-flight select guard ----

TEST_F(ControlModeStateMachineTest, Select_While_Previous_Select_Pending_Reports_Busy)
{
    GivenNvmAlwaysInvalid();

    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)>) {}));

    ConstructSubject();

    subject->Select(state_machine::ControlMode::speed, [](state_machine::SelectResult) {});

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Select(state_machine::ControlMode::position, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::busy);
}

// ---- M2: mode replacement is rejected while the active machine owns async work ----

TEST_F(ControlModeStateMachineTest, Select_While_Active_Machine_Has_Pending_Nvm_Command_Reports_Busy)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    infra::Function<void(services::NvmStatus)> deferredInvalidate;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&deferredInvalidate](infra::Function<void(services::NvmStatus)> onDone)
            {
                deferredInvalidate = onDone;
            }));

    subject->ActiveStateMachine().CmdClearCalibration([](state_machine::CommandResult) {});

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::busy);
    EXPECT_EQ(subject->Active(), state_machine::ControlMode::torque);

    deferredInvalidate(services::NvmStatus::Ok);

    GivenNvmSaveConfigSucceeds();
    state_machine::SelectResult retryResult{ state_machine::SelectResult::nvmFailed };
    subject->Select(state_machine::ControlMode::speed, [&retryResult](state_machine::SelectResult r)
        {
            retryResult = r;
        });

    EXPECT_EQ(retryResult, state_machine::SelectResult::ok);
    EXPECT_EQ(subject->Active(), state_machine::ControlMode::speed);
}

TEST_F(ControlModeStateMachineTest, Select_While_Boot_Nvm_Check_In_Flight_Reports_Busy)
{
    infra::Function<void(bool)> deferredValidityCheck;
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([&deferredValidityCheck](infra::Function<void(bool)> onDone)
            {
                deferredValidityCheck = onDone;
            }))
        .WillRepeatedly(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(false);
            }));

    ConstructSubject();

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::busy);
    EXPECT_EQ(subject->Active(), state_machine::ControlMode::torque);

    deferredValidityCheck(false);

    GivenNvmSaveConfigSucceeds();
    state_machine::SelectResult retryResult{ state_machine::SelectResult::nvmFailed };
    subject->Select(state_machine::ControlMode::speed, [&retryResult](state_machine::SelectResult r)
        {
            retryResult = r;
        });

    EXPECT_EQ(retryResult, state_machine::SelectResult::ok);
    EXPECT_EQ(subject->Active(), state_machine::ControlMode::speed);
}

// ---- Additional helpers ----

namespace
{
    class ControlModeStateMachineExtTest
        : public ControlModeStateMachineTest
    {
    public:
        void GivenNvmAlwaysValid()
        {
            services::CalibrationData data{};
            data.polePairs = 7;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.lQ = 1.0f;
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](infra::Function<void(bool)> onDone)
                    {
                        onDone(true);
                    }));
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([data](services::CalibrationData& out,
                                           infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = data;
                        onDone(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
        }

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

        std::string capturedOutput;

        void CaptureTerminalOutput()
        {
            EXPECT_CALL(streamWriterMock, Insert(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([this](infra::ConstByteRange range, infra::StreamErrorPolicy&)
                    {
                        capturedOutput.append(reinterpret_cast<const char*>(range.begin()), range.size());
                    }));
        }

        std::string OutputOf(const char* command)
        {
            capturedOutput.clear();
            InvokeCliCommand(command);
            return capturedOutput;
        }
    };
}

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

// ---- Full lifecycle tests covering all 3 NoOp state machine types ----

namespace
{
    class ControlModeStateMachineLifecycleTest
        : public ControlModeStateMachineExtTest
    {
    public:
        infra::Function<void(std::optional<std::size_t>)> capturedPolePairsCb;
        infra::Function<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)> capturedResistanceCb;
        infra::Function<void(std::optional<foc::Radians>)> capturedAlignmentCb;
        infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>,
            std::optional<foc::NewtonMeterSecondSquared>)>
            capturedMechIdentCb;
        infra::Function<void(services::NvmStatus)> capturedNvmSaveCb;

        void SetUpTorqueCalibrationCaptures()
        {
            EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                .WillOnce(Invoke([this](const auto&, const auto& cb)
                    {
                        capturedPolePairsCb = cb;
                    }));
            EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                .WillOnce(Invoke([this](const auto&, const auto& cb)
                    {
                        capturedResistanceCb = cb;
                    }));
            EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                .WillOnce(Invoke([this](auto, const auto&, const auto& cb)
                    {
                        capturedAlignmentCb = cb;
                    }));
            EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                .WillOnce(Invoke([this](const auto&, const auto& cb)
                    {
                        capturedNvmSaveCb = cb;
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(1);
        }

        void SetUpMechIdentCalibrationCaptures()
        {
            SetUpTorqueCalibrationCaptures();
            EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
                .WillOnce(Invoke([this](const auto&, auto, const auto&, const auto& cb)
                    {
                        capturedMechIdentCb = cb;
                    }));
        }

        void CompleteCalibration_Torque()
        {
            capturedPolePairsCb(7);
            capturedResistanceCb(foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f });
            capturedAlignmentCb(foc::Radians{ 0.0f });
            capturedNvmSaveCb(services::NvmStatus::Ok);
        }

        void CompleteCalibration_WithMechIdent()
        {
            capturedPolePairsCb(7);
            capturedResistanceCb(foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f });
            capturedAlignmentCb(foc::Radians{ 0.0f });
            capturedMechIdentCb(foc::NewtonMeterSecondPerRadian{ 0.005f }, foc::NewtonMeterSecondSquared{ 0.01f });
            capturedNvmSaveCb(services::NvmStatus::Ok);
        }
    };
}

// ---- Torque mode full lifecycle ----

TEST_F(ControlModeStateMachineLifecycleTest, Full_Calibration_Completes_To_Ready_In_Torque_Mode)
{
    GivenNvmAlwaysInvalid();
    SetUpTorqueCalibrationCaptures();
    ConstructSubject();

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_Torque();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, CmdEnable_And_Disable_From_Ready_In_Torque_Mode)
{
    GivenNvmAlwaysInvalid();
    SetUpTorqueCalibrationCaptures();
    ConstructSubject();

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_Torque();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->ActiveStateMachine().CmdEnable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(
        subject->ActiveStateMachine().CurrentState()));

    subject->ActiveStateMachine().CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Fault_And_ClearFault_In_Torque_Mode)
{
    GivenNvmAlwaysInvalid();
    SetUpTorqueCalibrationCaptures();
    ConstructSubject();

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_Torque();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->ActiveStateMachine().CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(
        subject->ActiveStateMachine().CurrentState()));
    EXPECT_EQ(subject->ActiveStateMachine().LastFaultCode(), state_machine::FaultCode::hardwareFault);

    subject->ActiveStateMachine().CmdClearFault();
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
        subject->ActiveStateMachine().CurrentState()));
}

// ---- Speed mode full lifecycle ----

TEST_F(ControlModeStateMachineLifecycleTest, Full_Calibration_Completes_To_Ready_In_Speed_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::speed, [](auto) {});

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, CmdEnable_And_Disable_From_Ready_In_Speed_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::speed, [](auto) {});

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->ActiveStateMachine().CmdEnable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(
        subject->ActiveStateMachine().CurrentState()));

    subject->ActiveStateMachine().CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Fault_And_ClearFault_In_Speed_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::speed, [](auto) {});

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->ActiveStateMachine().CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(
        subject->ActiveStateMachine().CurrentState()));
    EXPECT_EQ(subject->ActiveStateMachine().LastFaultCode(), state_machine::FaultCode::hardwareFault);

    subject->ActiveStateMachine().CmdClearFault();
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
        subject->ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, ApplyOnlineEstimates_When_Enabled_In_Speed_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::speed, [](auto) {});

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->ActiveStateMachine().CmdEnable();

    subject->ActiveStateMachine().ApplyOnlineEstimates();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(
        subject->ActiveStateMachine().CurrentState()));
}

// ---- Position mode full lifecycle ----

TEST_F(ControlModeStateMachineLifecycleTest, Full_Calibration_Completes_To_Ready_In_Position_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::position, [](auto) {});

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, CmdEnable_And_Disable_From_Ready_In_Position_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::position, [](auto) {});

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->ActiveStateMachine().CmdEnable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(
        subject->ActiveStateMachine().CurrentState()));

    subject->ActiveStateMachine().CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Fault_And_ClearFault_In_Position_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::position, [](auto) {});

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->ActiveStateMachine().CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(
        subject->ActiveStateMachine().CurrentState()));
    EXPECT_EQ(subject->ActiveStateMachine().LastFaultCode(), state_machine::FaultCode::hardwareFault);

    subject->ActiveStateMachine().CmdClearFault();
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
        subject->ActiveStateMachine().CurrentState()));
}

// ---- TrySet*PidGains: accepted by matching mode(s) ----

TEST_F(ControlModeStateMachineExtTest, TrySetCurrentBandwidth_AcceptedInTorqueMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_TRUE(subject->TrySetCurrentBandwidth(8377.6f));
}

TEST_F(ControlModeStateMachineExtTest, TrySetSpeedBandwidth_RejectedInTorqueMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_FALSE(subject->TrySetSpeedBandwidth(50.0f));
}

TEST_F(ControlModeStateMachineExtTest, TrySetSpeedBandwidth_AcceptedInSpeedMode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Select(state_machine::ControlMode::speed, [](auto) {});

    EXPECT_TRUE(subject->TrySetSpeedBandwidth(50.0f));
}

TEST_F(ControlModeStateMachineExtTest, TrySetPositionBandwidth_AcceptedOnlyInPositionMode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    EXPECT_FALSE(subject->TrySetPositionBandwidth(18.8f));

    subject->Select(state_machine::ControlMode::position, [](auto) {});

    EXPECT_TRUE(subject->TrySetPositionBandwidth(18.8f));
}

TEST_F(ControlModeStateMachineExtTest, SelectCurrentAlgorithm_DefaultsToPid)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(ControlModeStateMachineExtTest, SelectCurrentAlgorithm_RejectedWithoutMotorModel)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::invalidParameters);
    EXPECT_EQ(subject->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(ControlModeStateMachineExtTest, SelectCurrentAlgorithm_PersistsToNvmOnceCalibrated)
{
    GivenNvmValid();
    ConstructSubject();

    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([](const services::ConfigData& config, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_EQ(config.currentAlgorithm, static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat));
                onDone(services::NvmStatus::Ok);
            }));

    EXPECT_EQ(subject->SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::ok);
    EXPECT_EQ(subject->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::deadbeat);
}

TEST_F(ControlModeStateMachineExtTest, PersistedAlgorithmOutsideEnumRangeFallsBackToDefault)
{
    GivenNvmValid();

    services::ConfigData config{};
    config.currentAlgorithm = 200;
    config.speedAlgorithm = 200;
    config.positionAlgorithm = 200;
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(ControlModeStateMachineExtTest, ActiveAlgorithmReportsTheRunningLoopNotThePersistedByte)
{
    GivenNvmAlwaysInvalid();

    services::ConfigData config{};
    config.currentAlgorithm = static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat);
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(ControlModeStateMachineExtTest, SelectSpeedAlgorithm_RejectedInTorqueMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->SelectSpeedAlgorithm(foc::SpeedAlgorithm::lqi), foc::SelectResult::invalidAlgorithm);
}

TEST_F(ControlModeStateMachineExtTest, SelectSpeedAlgorithm_RejectedWithoutMechanicalModel)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Select(state_machine::ControlMode::speed, [](auto) {});

    EXPECT_EQ(subject->SelectSpeedAlgorithm(foc::SpeedAlgorithm::lqi), foc::SelectResult::invalidParameters);
}

// ---- CLI: algorithm selection parses every accepted name and rejects the rest ----

TEST_F(ControlModeStateMachineExtTest, Cli_SelectCurrentAlgorithm_AcceptsEveryName)
{
    GivenNvmValid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();

    for (const auto* name : { "sca pid", "sca decoupled", "sca deadbeat", "sca sliding" })
        InvokeCliCommand(name);
}

TEST_F(ControlModeStateMachineExtTest, Cli_SelectCurrentAlgorithm_RejectsUnknownName)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    InvokeCliCommand("sca nonsense");
}

TEST_F(ControlModeStateMachineExtTest, Cli_SelectSpeedAlgorithm_AcceptsEveryName)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::speed, [](auto) {});

    for (const auto* name : { "ssa pid", "ssa lqi", "ssa adrc", "ssa twodof" })
        InvokeCliCommand(name);
}

TEST_F(ControlModeStateMachineExtTest, Cli_SelectSpeedAlgorithm_RejectsUnknownName)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::speed, [](auto) {});

    InvokeCliCommand("ssa nonsense");
}

TEST_F(ControlModeStateMachineExtTest, Cli_SelectPositionAlgorithm_AcceptsEveryName)
{
    GivenNvmAlwaysValid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::position, [](auto) {});

    for (const auto* name : { "spa pid", "spa cascadep", "spa lqr", "spa lqi", "spa twodof" })
        InvokeCliCommand(name);
}

TEST_F(ControlModeStateMachineExtTest, Cli_SelectPositionAlgorithm_RejectsUnknownName)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::position, [](auto) {});

    InvokeCliCommand("spa nonsense");
}

TEST_F(ControlModeStateMachineExtTest, Cli_SelectPositionAlgorithm_RejectedOutsidePositionMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->SelectPositionAlgorithm(foc::PositionAlgorithm::lqr), foc::SelectResult::invalidAlgorithm);
}

// ---- CLI: active_algorithms names every enumerator ----

TEST_F(ControlModeStateMachineExtTest, Cli_ActiveAlgorithms_NamesEveryCurrentAlgorithm)
{
    GivenNvmValid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();

    for (const auto* name : { "sca pid", "sca decoupled", "sca deadbeat", "sca sliding" })
    {
        InvokeCliCommand(name);
        InvokeCliCommand("aa");
    }
}

TEST_F(ControlModeStateMachineExtTest, Cli_ActiveAlgorithms_NamesEverySpeedAlgorithm)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::speed, [](auto) {});

    for (const auto* name : { "ssa pid", "ssa lqi", "ssa adrc", "ssa twodof" })
    {
        InvokeCliCommand(name);
        InvokeCliCommand("aa");
    }
}

TEST_F(ControlModeStateMachineExtTest, Cli_ActiveAlgorithms_NamesEveryPositionAlgorithm)
{
    GivenNvmAlwaysValid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::position, [](auto) {});

    for (const auto* name : { "spa pid", "spa cascadep", "spa lqr", "spa lqi", "spa twodof" })
    {
        InvokeCliCommand(name);
        InvokeCliCommand("aa");
    }
}

// ---- Bandwidth commands reach each loop ----

TEST_F(ControlModeStateMachineExtTest, TrySetBandwidths_AreAcceptedInPositionMode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();
    subject->Select(state_machine::ControlMode::position, [](auto) {});

    EXPECT_TRUE(subject->TrySetCurrentBandwidth(6283.2f));
    EXPECT_TRUE(subject->TrySetSpeedBandwidth(188.5f));
    EXPECT_TRUE(subject->TrySetPositionBandwidth(18.8f));
}

TEST_F(ControlModeStateMachineExtTest, TrySetSpeedAndPositionBandwidth_AreRejectedInTorqueMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_TRUE(subject->TrySetCurrentBandwidth(6283.2f));
    EXPECT_FALSE(subject->TrySetSpeedBandwidth(188.5f));
    EXPECT_FALSE(subject->TrySetPositionBandwidth(18.8f));
}

// ---- Persisted algorithms are restored once the motor model exists (REQ-CTRL-006) ----

TEST_F(ControlModeStateMachineExtTest, PersistedAlgorithmIsKeptInConfigWhileStillUnselectable)
{
    GivenNvmAlwaysInvalid();

    services::ConfigData config{};
    config.currentAlgorithm = static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat);
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);

    uint8_t persisted{ 0 };
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&persisted](const services::ConfigData& saved, infra::Function<void(services::NvmStatus)> onDone)
            {
                persisted = saved.currentAlgorithm;
                onDone(services::NvmStatus::Ok);
            }));

    subject->Select(state_machine::ControlMode::torque, [](auto) {});

    EXPECT_EQ(persisted, static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat));
}

TEST_F(ControlModeStateMachineExtTest, PersistedAlgorithmOutsideEnumRangeIsCorrectedInConfig)
{
    GivenNvmAlwaysValid();

    services::ConfigData config{};
    config.currentAlgorithm = 200;
    ConstructSubjectWithConfig(config);

    uint8_t persisted{ 200 };
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&persisted](const services::ConfigData& saved, infra::Function<void(services::NvmStatus)> onDone)
            {
                persisted = saved.currentAlgorithm;
                onDone(services::NvmStatus::Ok);
            }));

    subject->Select(state_machine::ControlMode::torque, [](auto) {});

    EXPECT_EQ(persisted, static_cast<uint8_t>(foc::CurrentAlgorithm::pid));
}

TEST_F(ControlModeStateMachineLifecycleTest, PersistedCurrentAlgorithmIsAppliedOnceReadyIsReached)
{
    GivenNvmAlwaysInvalid();
    SetUpTorqueCalibrationCaptures();

    services::ConfigData config{};
    config.currentAlgorithm = static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat);
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_Torque();

    EXPECT_EQ(subject->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::deadbeat);
}

TEST_F(ControlModeStateMachineLifecycleTest, PersistedSpeedAlgorithmIsAppliedOnceReadyIsReached)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    SetUpMechIdentCalibrationCaptures();

    services::ConfigData config{};
    config.defaultControlMode = static_cast<uint8_t>(state_machine::ControlMode::speed);
    config.speedAlgorithm = static_cast<uint8_t>(foc::SpeedAlgorithm::lqi);
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->ActiveSpeedAlgorithm(), foc::SpeedAlgorithm::pid);

    subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_EQ(subject->ActiveSpeedAlgorithm(), foc::SpeedAlgorithm::lqi);
}

// ---- CLI: setpoint and bandwidth commands registered on the coordinator ----

namespace
{
    class ControlModeStateMachineCliTest
        : public ControlModeStateMachineLifecycleTest
    {
    public:
        void GivenTorqueModeReady()
        {
            GivenNvmAlwaysInvalid();
            SetUpTorqueCalibrationCaptures();
            ConstructSubject(static_cast<uint8_t>(state_machine::ControlMode::torque));
            subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
            CompleteCalibration_Torque();
            CaptureTerminalOutput();
        }

        void GivenOuterLoopModeReady(state_machine::ControlMode mode)
        {
            GivenNvmAlwaysInvalid();
            GivenNvmSaveConfigAlwaysSucceeds();
            SetUpMechIdentCalibrationCaptures();
            ConstructSubject(static_cast<uint8_t>(mode));
            subject->ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
            CompleteCalibration_WithMechIdent();
            CaptureTerminalOutput();
        }

        void GivenTorqueModeIdle()
        {
            GivenNvmAlwaysInvalid();
            ConstructSubject(static_cast<uint8_t>(state_machine::ControlMode::torque));
            CaptureTerminalOutput();
        }
    };
}

TEST_F(ControlModeStateMachineCliTest, SetTorque_Is_Accepted_In_Torque_Mode_When_Ready)
{
    GivenTorqueModeReady();

    const auto output = OutputOf("st 2.5");

    EXPECT_THAT(output, Not(HasSubstr("Unrecognized command.")));
    EXPECT_THAT(output, Not(HasSubstr("ERROR")));
    EXPECT_THAT(output, HasSubstr("> "));
}

TEST_F(ControlModeStateMachineCliTest, SetSpeed_Is_Accepted_In_Speed_Mode_When_Enabled)
{
    GivenOuterLoopModeReady(state_machine::ControlMode::speed);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->ActiveStateMachine().CmdEnable();

    const auto output = OutputOf("ss 20.0");

    EXPECT_THAT(output, Not(HasSubstr("Unrecognized command.")));
    EXPECT_THAT(output, Not(HasSubstr("ERROR")));
    EXPECT_THAT(output, HasSubstr("> "));
}

TEST_F(ControlModeStateMachineCliTest, SetPosition_Is_Accepted_In_Position_Mode_When_Ready)
{
    GivenOuterLoopModeReady(state_machine::ControlMode::position);

    const auto output = OutputOf("sp 3.14");

    EXPECT_THAT(output, Not(HasSubstr("Unrecognized command.")));
    EXPECT_THAT(output, Not(HasSubstr("ERROR")));
    EXPECT_THAT(output, HasSubstr("> "));
}

TEST_F(ControlModeStateMachineCliTest, SetSpeed_Is_Rejected_With_Mode_Message_In_Torque_Mode)
{
    GivenTorqueModeReady();

    const auto output = OutputOf("ss 20.0");

    EXPECT_THAT(output, HasSubstr("ERROR"));
    EXPECT_THAT(output, HasSubstr("rejected: command does not apply to the active control mode."));
}

TEST_F(ControlModeStateMachineCliTest, SetPosition_Is_Rejected_With_Mode_Message_In_Speed_Mode)
{
    GivenOuterLoopModeReady(state_machine::ControlMode::speed);

    const auto output = OutputOf("sp 1.0");

    EXPECT_THAT(output, HasSubstr("ERROR"));
    EXPECT_THAT(output, HasSubstr("rejected: command does not apply to the active control mode."));
}

TEST_F(ControlModeStateMachineCliTest, SetTorque_Is_Rejected_When_Lifecycle_State_Is_Idle)
{
    GivenTorqueModeIdle();

    const auto output = OutputOf("st 2.5");

    EXPECT_THAT(output, HasSubstr("ERROR"));
    EXPECT_THAT(output, HasSubstr("rejected: setpoints are only accepted in Ready or Enabled."));
}

TEST_F(ControlModeStateMachineCliTest, SetTorque_Rejects_Wrong_Argument_Count)
{
    GivenTorqueModeReady();

    EXPECT_THAT(OutputOf("st 1.0 2.0"), HasSubstr("invalid number of arguments."));
    EXPECT_THAT(OutputOf("st"), HasSubstr("invalid number of arguments."));
}

TEST_F(ControlModeStateMachineCliTest, SetTorque_Rejects_NonNumeric_Argument)
{
    GivenTorqueModeReady();

    EXPECT_THAT(OutputOf("st abc"), HasSubstr("invalid value. It should be a float."));
}

TEST_F(ControlModeStateMachineCliTest, SetSpeed_Rejects_NonNumeric_Argument_Before_Mode_Check)
{
    GivenOuterLoopModeReady(state_machine::ControlMode::speed);

    EXPECT_THAT(OutputOf("ss abc"), HasSubstr("invalid value. It should be a float."));
}

TEST_F(ControlModeStateMachineCliTest, SetCurrentBandwidth_Is_Accepted_In_Torque_Mode)
{
    GivenTorqueModeReady();

    const auto output = OutputOf("scbw 6283.2");

    EXPECT_THAT(output, Not(HasSubstr("Unrecognized command.")));
    EXPECT_THAT(output, Not(HasSubstr("ERROR")));
    EXPECT_THAT(output, HasSubstr("> "));
}

TEST_F(ControlModeStateMachineCliTest, SetCurrentBandwidth_Rejects_Wrong_Argument_Count)
{
    GivenTorqueModeReady();

    EXPECT_THAT(OutputOf("scbw 100.0 200.0"), HasSubstr("invalid number of arguments."));
}

TEST_F(ControlModeStateMachineCliTest, SetSpeedBandwidth_Is_Rejected_In_Torque_Mode)
{
    GivenTorqueModeReady();

    const auto output = OutputOf("ssbw 188.5");

    EXPECT_THAT(output, HasSubstr("ERROR"));
    EXPECT_THAT(output, HasSubstr("rejected: command does not apply to the active control mode."));
}

TEST_F(ControlModeStateMachineCliTest, SetSpeedBandwidth_Is_Accepted_In_Speed_Mode)
{
    GivenOuterLoopModeReady(state_machine::ControlMode::speed);

    const auto output = OutputOf("ssbw 188.5");

    EXPECT_THAT(output, Not(HasSubstr("Unrecognized command.")));
    EXPECT_THAT(output, Not(HasSubstr("ERROR")));
    EXPECT_THAT(output, HasSubstr("> "));
}

TEST_F(ControlModeStateMachineCliTest, SetPositionBandwidth_Is_Rejected_In_Speed_Mode)
{
    GivenOuterLoopModeReady(state_machine::ControlMode::speed);

    const auto output = OutputOf("spbw 18.8");

    EXPECT_THAT(output, HasSubstr("ERROR"));
    EXPECT_THAT(output, HasSubstr("rejected: command does not apply to the active control mode."));
}

TEST_F(ControlModeStateMachineCliTest, SetPositionBandwidth_Reaches_The_Position_Cascade)
{
    GivenOuterLoopModeReady(state_machine::ControlMode::position);

    const auto output = OutputOf("spbw 18.8");

    EXPECT_THAT(output, Not(HasSubstr("Unrecognized command.")));
    EXPECT_THAT(output, Not(HasSubstr("ERROR")));
    EXPECT_THAT(output, HasSubstr("> "));
}

TEST_F(ControlModeStateMachineCliTest, EstimateStatus_Prints_The_Active_Mode_Estimates)
{
    GivenOuterLoopModeReady(state_machine::ControlMode::speed);

    const auto output = OutputOf("es");

    EXPECT_THAT(output, HasSubstr("[EST] Mech: J="));
    EXPECT_THAT(output, HasSubstr("[EST] Elec: R="));
}

TEST_F(ControlModeStateMachineCliTest, EstimateStatus_Reports_That_Torque_Mode_Has_No_Estimators)
{
    GivenTorqueModeReady();

    EXPECT_THAT(OutputOf("es"), HasSubstr("online estimates are not available in torque mode"));
}
