#include "TestFocStateMachineHelper.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/services/cli/TerminalTorque.hpp"

namespace
{
    using namespace testing;

    class FocStateMachineTorqueCliTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using TestedStateMachine = application::FocStateMachineImpl<foc::FocTorqueImpl, services::TerminalFocTorqueInteractor>;

        StrictMock<test_helpers::StreamWriterMock> streamWriterMock;
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
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;

        foc::Volts vdc{ 24.0f };

        infra::Execute setupInverterExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
            } };

        void GivenNvmInvalid()
        {
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
                    {
                        onDone(false);
                    }));
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

        void GivenFaultNotifierRegistered()
        {
            EXPECT_CALL(faultNotifierMock, Register(_))
                .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
                    {
                        faultNotifierMock.StoreHandler(handler);
                    }));
        }

        void ExpectCalibrationSequence(bool polePairsOk = true,
            bool resistanceOk = true,
            bool alignmentOk = true,
            bool nvmSaveOk = true)
        {
            if (polePairsOk)
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::size_t{ 7 });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::nullopt);
                        }));
                return;
            }

            if (resistanceOk)
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(std::optional<foc::Ohm>,
                                             std::optional<foc::MilliHenry>)>& cb)
                        {
                            cb(foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(std::optional<foc::Ohm>,
                                             std::optional<foc::MilliHenry>)>& cb)
                        {
                            cb(std::nullopt, std::nullopt);
                        }));
                return;
            }

            if (alignmentOk)
                EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                    .WillOnce(Invoke([](std::size_t, const auto&,
                                         const infra::Function<void(std::optional<foc::Radians>)>& cb)
                        {
                            cb(foc::Radians{ 0.0f });
                        }));
            else
            {
                EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                    .WillOnce(Invoke([](std::size_t, const auto&,
                                         const infra::Function<void(std::optional<foc::Radians>)>& cb)
                        {
                            cb(std::nullopt);
                        }));
                return;
            }

            if (nvmSaveOk)
            {
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::Ok);
                        }));
                EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
            }
            else
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::WriteFailed);
                        }));
        }

        TestedStateMachine CreateStateMachine()
        {
            return TestedStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                inverterMock, encoderMock, vdc, nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock },
                faultNotifierMock
            };
        }
    };
}

// --- Boot / NVM auto-transitions ---

TEST_F(FocStateMachineTorqueCliTest, nvm_invalid_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, nvm_valid_on_boot_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, nvm_load_failure_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(true);
            }));
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([](services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::HardwareFault);
            }));
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Calibration success ---

TEST_F(FocStateMachineTorqueCliTest, calibrate_from_idle_runs_full_sequence_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, calibrate_from_ready_re_runs_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, calibrate_records_pole_pairs_in_ready_state)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    const auto& ready = std::get<state_machine::Ready>(sm.CurrentState());
    EXPECT_EQ(ready.loadedData.polePairs, uint8_t{ 7 });
}

TEST_F(FocStateMachineTorqueCliTest, calibrate_records_resistance_in_ready_state)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    const auto& ready = std::get<state_machine::Ready>(sm.CurrentState());
    EXPECT_NEAR(ready.loadedData.rPhase, 0.5f, 1e-5f);
}

// --- Calibration blocked from ENABLED ---

TEST_F(FocStateMachineTorqueCliTest, calibrate_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- Calibration failures ---

TEST_F(FocStateMachineTorqueCliTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code,
        state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachineTorqueCliTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, alignment_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, nvm_save_failure_after_full_calibration_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- Enable / Disable ---

TEST_F(FocStateMachineTorqueCliTest, enable_from_ready_starts_foc_and_enters_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, disable_from_enabled_stops_foc_and_enters_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, disable_from_ready_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Fault handling ---

TEST_F(FocStateMachineTorqueCliTest, fault_from_enabled_stops_pwm_and_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, fault_from_idle_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overvoltage);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, fault_from_ready_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::encoderLoss);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, fault_code_is_recorded)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code,
        state_machine::FaultCode::overcurrent);
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

// --- Clear fault ---

TEST_F(FocStateMachineTorqueCliTest, clear_fault_from_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, clear_fault_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- Clear calibration ---

TEST_F(FocStateMachineTorqueCliTest, clear_cal_from_ready_invalidates_nvm_and_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, clear_cal_from_idle_calls_invalidate)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- Safety: fault during calibration stops inverter ---

TEST_F(FocStateMachineTorqueCliTest, fault_during_calibration_stops_inverter_and_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    EXPECT_CALL(inverterMock, Stop()).Times(AtLeast(1));
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- CLI safety: start command must not bypass state machine guards ---

TEST_F(FocStateMachineTorqueCliTest, cli_start_command_does_not_bypass_idle_guard)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    communication.dataReceived(infra::MakeStringByteRange("start\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- CLI command invocation exercises RegisterCliCommands lambdas ---

TEST_F(FocStateMachineTorqueCliTest, cli_cal_command_triggers_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    communication.dataReceived(infra::MakeStringByteRange("cal\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, cli_en_command_enables_foc)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    communication.dataReceived(infra::MakeStringByteRange("en\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, cli_dis_command_disables_foc)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    communication.dataReceived(infra::MakeStringByteRange("dis\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, cli_cf_command_clears_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    communication.dataReceived(infra::MakeStringByteRange("cf\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueCliTest, cli_cc_command_clears_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    communication.dataReceived(infra::MakeStringByteRange("cc\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// ==========================================================================
// Torque-mode with AutoTransitionPolicy
// ==========================================================================

namespace
{
    class FocStateMachineTorqueAutoTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using AutoStateMachine = application::FocStateMachineImpl<foc::FocTorqueImpl,
            services::TerminalFocTorqueInteractor,
            state_machine::AutoTransitionPolicy>;

        StrictMock<test_helpers::StreamWriterMock> streamWriterMock;
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
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;

        foc::Volts vdc{ 24.0f };

        infra::Execute setupInverterExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
            } };

        void GivenFaultNotifierRegistered()
        {
            EXPECT_CALL(faultNotifierMock, Register(_))
                .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
                    {
                        faultNotifierMock.StoreHandler(handler);
                    }));
        }

        void GivenNvmInvalid()
        {
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
                    {
                        onDone(false);
                    }));
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

        void ExpectCalibrationSequence(bool polePairsOk = true,
            bool resistanceOk = true,
            bool alignmentOk = true,
            bool nvmSaveOk = true)
        {
            if (polePairsOk)
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::size_t{ 7 });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::nullopt);
                        }));
                return;
            }

            if (resistanceOk)
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(std::optional<foc::Ohm>,
                                             std::optional<foc::MilliHenry>)>& cb)
                        {
                            cb(foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(std::optional<foc::Ohm>,
                                             std::optional<foc::MilliHenry>)>& cb)
                        {
                            cb(std::nullopt, std::nullopt);
                        }));
                return;
            }

            if (alignmentOk)
                EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                    .WillOnce(Invoke([](std::size_t, const auto&,
                                         const infra::Function<void(std::optional<foc::Radians>)>& cb)
                        {
                            cb(foc::Radians{ 0.0f });
                        }));
            else
            {
                EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                    .WillOnce(Invoke([](std::size_t, const auto&,
                                         const infra::Function<void(std::optional<foc::Radians>)>& cb)
                        {
                            cb(std::nullopt);
                        }));
                return;
            }

            if (nvmSaveOk)
            {
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::Ok);
                        }));
                EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
            }
            else
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::WriteFailed);
                        }));
        }

        AutoStateMachine CreateStateMachine()
        {
            return AutoStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                inverterMock, encoderMock, vdc, nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock },
                faultNotifierMock
            };
        }
    };
}

// --- Boot ---

TEST_F(FocStateMachineTorqueAutoTest, starts_in_idle_when_nvm_invalid)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, nvm_valid_on_boot_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, nvm_load_failure_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(true);
            }));
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([](services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::HardwareFault);
            }));
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Calibration ---

TEST_F(FocStateMachineTorqueAutoTest, calibrate_enable_disable_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, calibrate_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- Calibration failures ---

TEST_F(FocStateMachineTorqueAutoTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, alignment_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, nvm_save_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- Enable / Disable guards ---

TEST_F(FocStateMachineTorqueAutoTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, disable_from_ready_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Fault handling ---

TEST_F(FocStateMachineTorqueAutoTest, fault_from_enabled_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachineTorqueAutoTest, fault_and_clear_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdClearFault();
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_fault_from_non_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Clear calibration ---

TEST_F(FocStateMachineTorqueAutoTest, clear_cal_from_ready_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}
