#include "TestFocStateMachineHelper.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/services/cli/TerminalSpeed.hpp"

namespace
{
    using namespace testing;

    class FocStateMachineSpeedCliTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using SpeedStateMachine = application::FocStateMachineImpl<
            foc::FocSpeedImpl,
            services::TerminalFocSpeedInteractor>;

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
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<services::MechanicalParametersIdentificationMock> mechIdentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;

        foc::Volts vdc{ 24.0f };

        infra::Execute setupInverterExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
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

        void GivenNvmValidWithSpeedGains()
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.lQ = 1.0f;
            data.kpVelocity = 0.25f;
            data.kiVelocity = 0.5f;

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

        void ExpectSpeedCalibrationSequence(bool polePairsOk = true,
            bool resistanceOk = true,
            bool alignmentOk = true,
            bool mechOk = true,
            bool nvmSaveOk = true)
        {
            if (polePairsOk)
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::size_t{ 4 });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(std::optional<std::size_t>)>& cb)
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

            if (mechOk)
            {
                EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
                    .WillOnce(Invoke([](const foc::NewtonMeter&,
                                         std::size_t,
                                         const services::MechanicalParametersIdentification::Config&,
                                         const infra::Function<void(
                                             std::optional<foc::NewtonMeterSecondPerRadian>,
                                             std::optional<foc::NewtonMeterSecondSquared>)>& cb)
                        {
                            cb(foc::NewtonMeterSecondPerRadian{ 0.01f },
                                foc::NewtonMeterSecondSquared{ 0.005f });
                        }));
            }
            else
            {
                EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
                    .WillOnce(Invoke([](const foc::NewtonMeter&,
                                         std::size_t,
                                         const services::MechanicalParametersIdentification::Config&,
                                         const infra::Function<void(
                                             std::optional<foc::NewtonMeterSecondPerRadian>,
                                             std::optional<foc::NewtonMeterSecondSquared>)>& cb)
                        {
                            cb(std::nullopt, std::nullopt);
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

        SpeedStateMachine CreateSpeedStateMachine()
        {
            return SpeedStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
                faultNotifierMock,
                foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock
            };
        }
    };
}

// --- Boot / NVM ---

TEST_F(FocStateMachineSpeedCliTest, nvm_invalid_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, nvm_valid_on_boot_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, nvm_load_failure_on_boot_remains_in_idle)
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
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Calibration success ---

TEST_F(FocStateMachineSpeedCliTest, calibration_calls_mech_ident_after_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, calibration_populates_inertia_and_velocity_gains)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    const auto& data = std::get<state_machine::Ready>(sm.CurrentState()).loadedData;
    EXPECT_NEAR(data.inertia, 0.005f, 1e-5f);
    EXPECT_NEAR(data.frictionViscous, 0.01f, 1e-5f);
    EXPECT_GT(data.kpVelocity, 0.0f);
    EXPECT_GT(data.kiVelocity, 0.0f);
}

TEST_F(FocStateMachineSpeedCliTest, calibrate_from_ready_re_runs_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, calibrate_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- Calibration failures ---

TEST_F(FocStateMachineSpeedCliTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, alignment_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, mech_ident_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code,
        state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachineSpeedCliTest, nvm_save_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, true, true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- Enable / Disable ---

TEST_F(FocStateMachineSpeedCliTest, enable_from_ready_enters_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, disable_from_enabled_returns_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, disable_from_ready_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Fault handling ---

TEST_F(FocStateMachineSpeedCliTest, fault_from_enabled_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, fault_from_idle_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, fault_code_is_recorded)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

// --- Clear fault ---

TEST_F(FocStateMachineSpeedCliTest, clear_fault_from_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_fault_from_non_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Clear calibration ---

TEST_F(FocStateMachineSpeedCliTest, clear_cal_from_ready_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- CLI command invocation exercises RegisterCliCommands lambdas ---

TEST_F(FocStateMachineSpeedCliTest, cli_cal_command_triggers_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    communication.dataReceived(infra::MakeStringByteRange("cal\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, cli_en_command_enables_foc)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    communication.dataReceived(infra::MakeStringByteRange("en\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, cli_dis_command_disables_foc)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    communication.dataReceived(infra::MakeStringByteRange("dis\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, cli_cf_command_clears_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    communication.dataReceived(infra::MakeStringByteRange("cf\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, cli_cc_command_clears_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    communication.dataReceived(infra::MakeStringByteRange("cc\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Async callback safety: late callbacks after fault must be silently ignored ---

TEST_F(FocStateMachineSpeedCliTest, late_pole_pairs_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<std::size_t>)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([&capturedCb](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(std::size_t{ 7 });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, late_alignment_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<foc::Radians>)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(std::optional<foc::Ohm>,
                                 std::optional<foc::MilliHenry>)>& cb)
            {
                cb(foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([&capturedCb](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(foc::Radians{ 0.0f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, late_mech_ident_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>,
        std::optional<foc::NewtonMeterSecondSquared>)>
        capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(std::optional<foc::Ohm>,
                                 std::optional<foc::MilliHenry>)>& cb)
            {
                cb(foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([&capturedCb](const foc::NewtonMeter&,
                             std::size_t,
                             const services::MechanicalParametersIdentification::Config&,
                             const infra::Function<void(
                                 std::optional<foc::NewtonMeterSecondPerRadian>,
                                 std::optional<foc::NewtonMeterSecondSquared>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 0.005f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- CmdClearCalibration safety ---

TEST_F(FocStateMachineSpeedCliTest, clear_cal_during_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_cal_nvm_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::WriteFailed);
            }));
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- ApplyOnlineEstimates ---

TEST_F(FocStateMachineSpeedCliTest, apply_online_estimates_does_not_change_state_when_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.ApplyOnlineEstimates();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, apply_online_estimates_is_ignored_when_not_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    sm.ApplyOnlineEstimates();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, cli_ae_command_applies_estimates_when_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    communication.dataReceived(infra::MakeStringByteRange("ae\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// ==========================================================================
// Speed-mode with AutoTransitionPolicy
// ==========================================================================

namespace
{
    class FocStateMachineSpeedAutoTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using SpeedAutoStateMachine = application::FocStateMachineImpl<
            foc::FocSpeedImpl,
            services::TerminalFocSpeedInteractor,
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
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<services::MechanicalParametersIdentificationMock> mechIdentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;

        foc::Volts vdc{ 24.0f };

        infra::Execute setupInverterExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
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

        void GivenNvmValidWithSpeedGains()
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.lQ = 1.0f;
            data.kpVelocity = 0.25f;
            data.kiVelocity = 0.5f;

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

        void ExpectSpeedCalibrationSequence(bool polePairsOk = true,
            bool resistanceOk = true,
            bool alignmentOk = true,
            bool mechOk = true,
            bool nvmSaveOk = true)
        {
            if (polePairsOk)
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::size_t{ 4 });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(std::optional<std::size_t>)>& cb)
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

            if (mechOk)
            {
                EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
                    .WillOnce(Invoke([](const foc::NewtonMeter&,
                                         std::size_t,
                                         const services::MechanicalParametersIdentification::Config&,
                                         const infra::Function<void(
                                             std::optional<foc::NewtonMeterSecondPerRadian>,
                                             std::optional<foc::NewtonMeterSecondSquared>)>& cb)
                        {
                            cb(foc::NewtonMeterSecondPerRadian{ 0.01f },
                                foc::NewtonMeterSecondSquared{ 0.005f });
                        }));
            }
            else
            {
                EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
                    .WillOnce(Invoke([](const foc::NewtonMeter&,
                                         std::size_t,
                                         const services::MechanicalParametersIdentification::Config&,
                                         const infra::Function<void(
                                             std::optional<foc::NewtonMeterSecondPerRadian>,
                                             std::optional<foc::NewtonMeterSecondSquared>)>& cb)
                        {
                            cb(std::nullopt, std::nullopt);
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

        SpeedAutoStateMachine CreateSpeedAutoStateMachine()
        {
            return SpeedAutoStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
                faultNotifierMock,
                foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock
            };
        }
    };
}

// --- Boot ---

TEST_F(FocStateMachineSpeedAutoTest, starts_in_idle_when_nvm_invalid)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, nvm_valid_on_boot_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, nvm_load_failure_on_boot_remains_in_idle)
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
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Calibration ---

TEST_F(FocStateMachineSpeedAutoTest, calibrate_enable_disable_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, calibrate_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- Calibration failures ---

TEST_F(FocStateMachineSpeedAutoTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, alignment_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, mech_ident_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, true, false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, nvm_save_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, true, true, false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- Enable / Disable guards ---

TEST_F(FocStateMachineSpeedAutoTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, disable_from_ready_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Fault handling ---

TEST_F(FocStateMachineSpeedAutoTest, fault_from_enabled_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachineSpeedAutoTest, fault_and_clear_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Clear calibration ---

TEST_F(FocStateMachineSpeedAutoTest, clear_cal_from_ready_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}
