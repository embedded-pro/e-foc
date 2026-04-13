#include "TestFocStateMachineHelper.hpp"
#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/services/cli/TerminalPosition.hpp"

namespace
{
    using namespace testing;

    class FocStateMachinePositionCliTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using PositionStateMachine = application::FocStateMachineImpl<
            foc::FocPositionImpl,
            services::TerminalFocPositionInteractor>;

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

        void GivenNvmValidWithPositionGains()
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

        void ExpectPositionCalibrationSequence(bool polePairsOk = true,
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

        PositionStateMachine CreatePositionStateMachine()
        {
            return PositionStateMachine{
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

TEST_F(FocStateMachinePositionCliTest, nvm_invalid_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreatePositionStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, nvm_valid_on_boot_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, nvm_load_failure_on_boot_remains_in_idle)
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
    auto sm = CreatePositionStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Calibration success ---

TEST_F(FocStateMachinePositionCliTest, calibration_calls_mech_ident_after_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence();
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, calibration_populates_inertia_and_velocity_gains)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence();
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    const auto& data = std::get<state_machine::Ready>(sm.CurrentState()).loadedData;
    EXPECT_NEAR(data.inertia, 0.005f, 1e-5f);
    EXPECT_NEAR(data.frictionViscous, 0.01f, 1e-5f);
    EXPECT_GT(data.kpVelocity, 0.0f);
    EXPECT_GT(data.kiVelocity, 0.0f);
}

TEST_F(FocStateMachinePositionCliTest, calibrate_from_ready_re_runs_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    ExpectPositionCalibrationSequence();
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, calibrate_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, no_mech_ident_override_enters_fault)
{
    EXPECT_CALL(faultNotifierMock, Register(_))
        .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
            {
                faultNotifierMock.StoreHandler(handler);
            }));
    GivenNvmInvalid();

    PositionStateMachine sm{
        application::TerminalAndTracer{ terminal, tracer },
        application::MotorHardware{ inverterMock, encoderMock, vdc },
        nvmMock,
        application::CalibrationServices{ electricalIdentMock, alignmentMock },
        faultNotifierMock,
        foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock
    };

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 4 });
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

    sm.CmdCalibrate();

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code,
        state_machine::FaultCode::calibrationFailed);
}

// --- Calibration failures ---

TEST_F(FocStateMachinePositionCliTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(false);
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(true, false);
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, alignment_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(true, true, false);
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, mech_ident_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(true, true, true, false);
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code,
        state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachinePositionCliTest, nvm_save_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(true, true, true, true, false);
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- Enable / Disable ---

TEST_F(FocStateMachinePositionCliTest, enable_disable_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreatePositionStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, disable_from_ready_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Fault handling ---

TEST_F(FocStateMachinePositionCliTest, fault_from_enabled_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, fault_from_idle_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreatePositionStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, fault_code_is_recorded)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreatePositionStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

// --- Clear fault ---

TEST_F(FocStateMachinePositionCliTest, clear_fault_from_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreatePositionStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, clear_fault_from_non_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Clear calibration ---

TEST_F(FocStateMachinePositionCliTest, clear_cal_from_ready_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- CLI command invocation exercises RegisterCliCommands lambdas ---

TEST_F(FocStateMachinePositionCliTest, cli_cal_command_triggers_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence();
    auto sm = CreatePositionStateMachine();

    communication.dataReceived(infra::MakeStringByteRange("cal\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, cli_en_command_enables_foc)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    communication.dataReceived(infra::MakeStringByteRange("en\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, cli_dis_command_disables_foc)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    communication.dataReceived(infra::MakeStringByteRange("dis\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, cli_cf_command_clears_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreatePositionStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    communication.dataReceived(infra::MakeStringByteRange("cf\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionCliTest, cli_cc_command_clears_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

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
// Position-mode with AutoTransitionPolicy
// ==========================================================================

namespace
{
    class FocStateMachinePositionAutoTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using PositionAutoStateMachine = application::FocStateMachineImpl<
            foc::FocPositionImpl,
            services::TerminalFocPositionInteractor,
            state_machine::AutoTransitionPolicy>;

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

        void GivenNvmValidWithPositionGains()
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

        void ExpectPositionCalibrationSequence(bool polePairsOk = true,
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

        PositionAutoStateMachine CreatePositionAutoStateMachine()
        {
            return PositionAutoStateMachine{
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

TEST_F(FocStateMachinePositionAutoTest, starts_in_idle_when_nvm_invalid)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreatePositionAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoTest, nvm_valid_on_boot_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoTest, nvm_load_failure_on_boot_remains_in_idle)
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
    auto sm = CreatePositionAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Calibration ---

TEST_F(FocStateMachinePositionAutoTest, calibrate_enable_disable_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence();
    auto sm = CreatePositionAutoStateMachine();

    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoTest, calibrate_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionAutoStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- Calibration failures ---

TEST_F(FocStateMachinePositionAutoTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(false);
    auto sm = CreatePositionAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(true, false);
    auto sm = CreatePositionAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoTest, alignment_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(true, true, false);
    auto sm = CreatePositionAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoTest, mech_ident_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(true, true, true, false);
    auto sm = CreatePositionAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoTest, nvm_save_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(true, true, true, true, false);
    auto sm = CreatePositionAutoStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- Enable / Disable guards ---

TEST_F(FocStateMachinePositionAutoTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreatePositionAutoStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoTest, disable_from_ready_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionAutoStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Fault handling ---

TEST_F(FocStateMachinePositionAutoTest, fault_from_enabled_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionAutoStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachinePositionAutoTest, fault_and_clear_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreatePositionAutoStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Clear calibration ---

TEST_F(FocStateMachinePositionAutoTest, clear_cal_from_ready_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionAutoStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionAutoStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}
