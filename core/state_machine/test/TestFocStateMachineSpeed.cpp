#include "TestFocStateMachineHelper.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include <limits>

namespace
{
    using namespace testing;

    class FocStateMachineSpeedCliTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using SpeedStateMachine = application::SpeedStateMachine;

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
        infra::Execute setupTeardownExpectations{ [this]()
            {
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(electricalIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
            } };

        foc::Volts vdc{ 24.0f };

        infra::Execute setupInverterExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Unregister()).Times(AnyNumber());
            } };

        void GivenFaultNotifierRegistered()
        {
            EXPECT_CALL(faultNotifierMock, Register(_, _))
                .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& immediate, const infra::Function<void(state_machine::FaultCode)>& deferred)
                    {
                        faultNotifierMock.StoreHandler(immediate, deferred);
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
            data.speedLoopBandwidth = 50.0f;
            data.inertia = 0.005f;
            data.frictionViscous = 0.0f;
            data.stage = services::CalibrationStage::complete;

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
                                         const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                        {
                            cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                        {
                            cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{});
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
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
                faultNotifierMock,
                state_machine::TransitionPolicy::Cli,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock }
            };
        }

        void AlignAfterBoot(SpeedStateMachine& sm)
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                .WillOnce(Invoke([](std::size_t, const auto&,
                                     const infra::Function<void(std::optional<foc::Radians>)>& cb)
                    {
                        cb(foc::Radians{ 0.0f });
                    }));
            EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                .WillOnce(Invoke([](const services::CalibrationData&,
                                     infra::Function<void(services::NvmStatus)> onDone)
                    {
                        onDone(services::NvmStatus::Ok);
                    }));
            sm.CmdReAlign([](state_machine::CommandResult) {});
        }

        void GivenNvmValidWithSpeedGainsAndInertia()
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.lQ = 1.0f;
            data.speedLoopBandwidth = 50.0f;
            data.inertia = 0.01f;
            data.frictionViscous = 0.005f;
            data.stage = services::CalibrationStage::complete;

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

        void GivenNvmValidWithZeroResistance()
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.0f;
            data.lD = 1.0f;
            data.lQ = 1.0f;
            data.speedLoopBandwidth = 50.0f;
            data.stage = services::CalibrationStage::complete;

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

        void GivenNvmValidWithMechanics(float inertia, float frictionViscous)
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.lQ = 1.0f;
            data.speedLoopBandwidth = 50.0f;
            data.inertia = inertia;
            data.frictionViscous = frictionViscous;
            data.stage = services::CalibrationStage::complete;

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

        void ExpectExternalCalibrationPersistsStage(services::CalibrationStage expected)
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                .WillOnce(Invoke([](std::size_t, const auto&,
                                     const infra::Function<void(std::optional<foc::Radians>)>& cb)
                    {
                        cb(foc::Radians{ 0.0f });
                    }));
            EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                .WillOnce(Invoke([expected](const services::CalibrationData& data,
                                     infra::Function<void(services::NvmStatus)> onDone)
                    {
                        EXPECT_EQ(data.stage, expected);
                        onDone(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
        }

        static services::CalibrationData ElectricalOnlyData()
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.lQ = 1.0f;
            return data;
        }
    };
}

TEST_F(FocStateMachineSpeedCliTest, nvm_invalid_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, nvm_valid_on_boot_remains_in_idle_pending_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
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

TEST_F(FocStateMachineSpeedCliTest, calibration_calls_mech_ident_after_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, calibration_populates_inertia_and_velocity_gains)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    const auto& data = std::get<state_machine::Ready>(sm.CurrentState()).loadedData;
    EXPECT_NEAR(data.inertia, 0.005f, 1e-5f);
    EXPECT_NEAR(data.frictionViscous, 0.01f, 1e-5f);
    EXPECT_GT(data.speedLoopBandwidth, 0.0f);
}

TEST_F(FocStateMachineSpeedCliTest, calibrate_from_ready_re_runs_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, calibrate_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, alignment_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, mech_ident_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

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

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, enable_from_ready_enters_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, disable_from_enabled_returns_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

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

TEST_F(FocStateMachineSpeedCliTest, disable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, fault_from_enabled_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

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

TEST_F(FocStateMachineSpeedCliTest, clear_fault_from_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_fault_from_fault_with_valid_calibration_returns_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_fault_from_non_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

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
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

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
    AlignAfterBoot(sm);

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
    AlignAfterBoot(sm);

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
    sm.CmdCalibrate([](state_machine::CommandResult) {});
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
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([&capturedCb](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
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
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
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
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 0.005f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_cal_during_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdClearCalibration([](state_machine::CommandResult) {});

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
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, late_resistance_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 4 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([&capturedCb](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, late_nvm_save_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 4 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([](const foc::NewtonMeter&,
                             std::size_t,
                             const services::MechanicalParametersIdentification::Config&,
                             const infra::Function<void(
                                 std::optional<foc::NewtonMeterSecondPerRadian>,
                                 std::optional<foc::NewtonMeterSecondSquared>)>& cb)
            {
                cb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 0.005f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&capturedCb](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, calibrate_is_rejected_until_the_boot_check_has_completed)
{
    GivenFaultNotifierRegistered();
    infra::Function<void(bool)> bootCb;
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([&bootCb](infra::Function<void(bool)> onDone)
            {
                bootCb = onDone;
            }));
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    std::optional<state_machine::CommandResult> result;
    sm.CmdCalibrate([&result](state_machine::CommandResult value)
        {
            result = value;
        });
    EXPECT_EQ(state_machine::CommandResult::rejected, result);
    ASSERT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));

    bootCb(false);
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, enable_is_rejected_while_a_clear_calibration_is_outstanding)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_EQ(state_machine::CommandResult::rejected, sm.CmdEnable());
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    capturedCb(services::NvmStatus::Ok);
    EXPECT_EQ(state_machine::CommandResult::ok, result);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_cal_invalidated_after_a_fault_drops_the_record_so_clear_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, result);

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPendingAsyncWork());

    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdClearFault());
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_cal_invalidate_failure_callback_after_fault_does_not_re_enter_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    ASSERT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);

    capturedCb(services::NvmStatus::WriteFailed);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachineSpeedCliTest, apply_online_estimates_does_not_change_state_when_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

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

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, cli_ae_command_applies_estimates_when_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    communication.dataReceived(infra::MakeStringByteRange("ae\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

namespace
{
    class FocStateMachineSpeedAutoTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using SpeedAutoStateMachine = application::SpeedStateMachine;

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
        infra::Execute setupTeardownExpectations{ [this]()
            {
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(electricalIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
            } };

        foc::Volts vdc{ 24.0f };

        infra::Execute setupInverterExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Unregister()).Times(AnyNumber());
            } };

        void GivenFaultNotifierRegistered()
        {
            EXPECT_CALL(faultNotifierMock, Register(_, _))
                .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& immediate, const infra::Function<void(state_machine::FaultCode)>& deferred)
                    {
                        faultNotifierMock.StoreHandler(immediate, deferred);
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
            data.speedLoopBandwidth = 50.0f;
            data.inertia = 0.005f;
            data.frictionViscous = 0.0f;
            data.stage = services::CalibrationStage::complete;

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
                                         const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                        {
                            cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                        {
                            cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{});
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
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
                faultNotifierMock,
                state_machine::TransitionPolicy::Auto,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock }
            };
        }

        void AlignAfterBoot(SpeedAutoStateMachine& sm)
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                .WillOnce(Invoke([](std::size_t, const auto&,
                                     const infra::Function<void(std::optional<foc::Radians>)>& cb)
                    {
                        cb(foc::Radians{ 0.0f });
                    }));
            EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                .WillOnce(Invoke([](const services::CalibrationData&,
                                     infra::Function<void(services::NvmStatus)> onDone)
                    {
                        onDone(services::NvmStatus::Ok);
                    }));
            sm.CmdReAlign([](state_machine::CommandResult) {});
        }
    };
}

TEST_F(FocStateMachineSpeedAutoTest, starts_in_idle_when_nvm_invalid)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, nvm_valid_on_boot_remains_in_idle_pending_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
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

TEST_F(FocStateMachineSpeedAutoTest, calibrate_enable_disable_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});
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
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, alignment_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, mech_ident_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, true, false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, nvm_save_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, true, true, false);
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, disable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, fault_from_enabled_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();
    AlignAfterBoot(sm);

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

TEST_F(FocStateMachineSpeedAutoTest, clear_fault_from_fault_with_valid_calibration_returns_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

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
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, late_pole_pairs_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<std::size_t>)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([&capturedCb](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(std::size_t{ 4 });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, late_resistance_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 4 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([&capturedCb](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, late_alignment_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<foc::Radians>)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 4 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([&capturedCb](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(foc::Radians{ 0.0f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, late_mech_ident_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>,
        std::optional<foc::NewtonMeterSecondSquared>)>
        capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 4 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
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
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 0.005f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, late_nvm_save_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 4 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([](const foc::NewtonMeter&,
                             std::size_t,
                             const services::MechanicalParametersIdentification::Config&,
                             const infra::Function<void(
                                 std::optional<foc::NewtonMeterSecondPerRadian>,
                                 std::optional<foc::NewtonMeterSecondSquared>)>& cb)
            {
                cb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 0.005f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&capturedCb](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, calibrate_is_rejected_until_the_boot_check_has_completed)
{
    GivenFaultNotifierRegistered();
    infra::Function<void(bool)> bootCb;
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([&bootCb](infra::Function<void(bool)> onDone)
            {
                bootCb = onDone;
            }));
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedAutoStateMachine();

    std::optional<state_machine::CommandResult> result;
    sm.CmdCalibrate([&result](state_machine::CommandResult value)
        {
            result = value;
        });
    EXPECT_EQ(state_machine::CommandResult::rejected, result);
    ASSERT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));

    bootCb(false);
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_cal_during_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_cal_nvm_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::WriteFailed);
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, enable_is_rejected_while_a_clear_calibration_is_outstanding)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();
    AlignAfterBoot(sm);

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_EQ(state_machine::CommandResult::rejected, sm.CmdEnable());
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    capturedCb(services::NvmStatus::Ok);
    EXPECT_EQ(state_machine::CommandResult::ok, result);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_cal_invalidated_after_a_fault_drops_the_record_so_clear_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, result);

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPendingAsyncWork());

    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdClearFault());
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_cal_invalidate_failure_callback_after_fault_does_not_re_enter_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    ASSERT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);

    capturedCb(services::NvmStatus::WriteFailed);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachineSpeedCliTest, calibrate_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, calibrate_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, calibrate_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, calibrate_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, enable_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, enable_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, enable_from_enabled_does_not_call_start_again)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, enable_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, enable_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, enable_from_enabled_does_not_call_start_again)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, disable_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, disable_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, disable_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, disable_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_fault_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_fault_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_fault_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_fault_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_fault_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateSpeedAutoStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_fault_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, clear_cal_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoTest, clear_cal_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedAutoStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, apply_mechanical_estimates_applies_when_physical_values)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGainsAndInertia();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.ApplyOnlineEstimates();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, apply_online_estimates_does_not_crash_when_called_while_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.ApplyOnlineEstimates();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, nvm_zero_resistance_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithZeroResistance();
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedCliTest, nvm_zero_inertia_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.0f, 0.01f);
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedCliTest, nvm_non_finite_inertia_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(std::numeric_limits<float>::quiet_NaN(), 0.01f);
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedCliTest, nvm_negative_friction_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.005f, -0.01f);
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedCliTest, nvm_non_finite_friction_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.005f, std::numeric_limits<float>::infinity());
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedCliTest, nvm_zero_friction_is_valid_and_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.005f, 0.0f);
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, external_calibration_without_mechanics_stays_idle_with_partial_record)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    ExpectExternalCalibrationPersistsStage(services::CalibrationStage::none);

    EXPECT_EQ(sm.CmdReserveExternalCalibration(), state_machine::CommandResult::ok);

    auto result = state_machine::CommandResult::rejected;
    sm.CmdCompleteExternalCalibration(ElectricalOnlyData(), [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_TRUE(sm.HasPartialCalibration());
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedCliTest, complete_external_calibration_without_reserve_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    auto result = state_machine::CommandResult::ok;
    sm.CmdCompleteExternalCalibration(ElectricalOnlyData(), [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::rejected);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPartialCalibration());
}

TEST_F(FocStateMachineSpeedCliTest, reserve_external_calibration_is_rejected_while_calibrating)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    EXPECT_EQ(sm.CmdReserveExternalCalibration(), state_machine::CommandResult::ok);
    EXPECT_EQ(sm.CmdReserveExternalCalibration(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedAutoTest, apply_online_estimates_does_not_change_state_when_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedAutoStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.ApplyOnlineEstimates();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedCliTest, has_pending_async_work_true_when_mech_ident_is_running)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();
    state_machine::FocStateMachineBase& base = sm;

    EXPECT_CALL(mechIdentMock, IsRunning()).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_TRUE(base.HasPendingAsyncWork());
}

TEST_F(FocStateMachineSpeedCliTest, has_pending_async_work_true_when_electrical_ident_is_running)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();
    state_machine::FocStateMachineBase& base = sm;

    EXPECT_CALL(electricalIdentMock, IsRunning()).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_TRUE(base.HasPendingAsyncWork());
}

namespace
{
    // Records what the state machine pushes into the live cascade, so a test can tell whether the loops were
    // configured before identification was allowed to excite them, and what was left behind afterwards.
    class RecordingSpeedCascade
        : public foc::SpeedCascade
    {
    public:
        using foc::SpeedCascade::SpeedCascade;

        bool Configure(const foc::MotorModelParameters& parameters) override
        {
            ++electricalConfigurations;
            lastElectrical = parameters;
            return foc::SpeedCascade::Configure(parameters);
        }

        bool ConfigureMechanics(const foc::MechanicalModelParameters& parameters) override
        {
            ++mechanicalConfigurations;
            lastMechanical = parameters;
            return foc::SpeedCascade::ConfigureMechanics(parameters);
        }

        std::size_t electricalConfigurations{ 0 };
        std::size_t mechanicalConfigurations{ 0 };
        foc::MotorModelParameters lastElectrical{};
        foc::MechanicalModelParameters lastMechanical{};
    };

    using RecordingSpeedController = foc::FocController<RecordingSpeedCascade>;

    // Refuses the plant it is handed while still being a real cascade, so a test can check what the state
    // machine leaves behind when the controller rejects the pending electrical model.
    class RefusingSpeedCascade
        : public RecordingSpeedCascade
    {
    public:
        using RecordingSpeedCascade::RecordingSpeedCascade;

        bool Configure(const foc::MotorModelParameters& parameters) override
        {
            RecordingSpeedCascade::Configure(parameters);
            return !refuseNextConfigure;
        }

        bool refuseNextConfigure{ false };
    };

    using RefusingSpeedController = foc::FocController<RefusingSpeedCascade>;

    class FocStateMachineSpeedIdentificationTest
        : public FocStateMachineSpeedCliTest
    {
    public:
        using RecordingStateMachine = application::OuterLoopStateMachineFor<RecordingSpeedController, RecordingSpeedController>;

        foc::NewtonMeter torqueConstant{ 0.1f };

        using RefusingStateMachine = application::OuterLoopStateMachineFor<RefusingSpeedController, RefusingSpeedController>;

        RefusingStateMachine CreateRefusingStateMachine()
        {
            return RefusingStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), torqueConstant },
                faultNotifierMock,
                state_machine::TransitionPolicy::Cli,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock }
            };
        }

        RecordingStateMachine CreateRecordingStateMachine()
        {
            return RecordingStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), torqueConstant },
                faultNotifierMock,
                state_machine::TransitionPolicy::Cli,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock }
            };
        }

        void GivenElectricalStepsSucceed()
        {
            EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
                    {
                        cb(std::size_t{ 4 });
                    }));
            EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                .WillOnce(Invoke([](const auto&, const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                    {
                        cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
                    }));
            EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
                    {
                        cb(foc::Radians{ 0.0f });
                    }));
        }
    };
}

TEST_F(FocStateMachineSpeedIdentificationTest, identification_only_starts_once_the_measured_model_and_a_provisional_plant_are_live)
{
    struct Observed
    {
        std::size_t electricalConfigurations = 0;
        std::size_t mechanicalConfigurations = 0;
        float resistance = 0.0f;
        float provisionalInertia = 0.0f;
    } observed;

    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    GivenElectricalStepsSucceed();

    auto sm = CreateRecordingStateMachine();
    auto& cascade = sm.GetController();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([&observed, &cascade](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
            {
                observed.electricalConfigurations = cascade.electricalConfigurations;
                observed.mechanicalConfigurations = cascade.mechanicalConfigurations;
                observed.resistance = cascade.lastElectrical.resistance.Value();
                observed.provisionalInertia = cascade.lastMechanical.inertia.Value();
                cb(std::nullopt, std::nullopt);
            }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_EQ(observed.electricalConfigurations, 1u);
    EXPECT_EQ(observed.mechanicalConfigurations, 1u);
    EXPECT_NEAR(observed.resistance, 0.5f, 1e-6f);
    EXPECT_GT(observed.provisionalInertia, 0.0f);
}

TEST_F(FocStateMachineSpeedIdentificationTest, identification_is_bounded_by_the_drive_current_envelope_and_a_speed_limit)
{
    services::MechanicalParametersIdentification::Config observed{};

    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    GivenElectricalStepsSucceed();

    auto sm = CreateRecordingStateMachine();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([&observed](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config& config, const auto& cb)
            {
                observed = config;
                cb(std::nullopt, std::nullopt);
            }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_GT(observed.maxCurrent.Value(), 10.0f);
    EXPECT_LE(observed.maxCurrent.Value(), 12.0f);
    EXPECT_GT(observed.maxSpeed.Value(), observed.targetSpeed.Value());
    EXPECT_LT(observed.dwellSpeed.Value(), observed.targetSpeed.Value());
    EXPECT_TRUE(services::IsUsableIdentificationConfig(observed));
}

TEST_F(FocStateMachineSpeedIdentificationTest, identification_never_runs_when_the_speed_loop_cannot_be_configured)
{
    torqueConstant = foc::NewtonMeter{ 0.0f };

    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    GivenElectricalStepsSucceed();

    auto sm = CreateRecordingStateMachine();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _)).Times(0);
    EXPECT_CALL(nvmMock, SaveCalibration(_, _)).Times(0);

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code, state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachineSpeedIdentificationTest, an_implausible_identified_inertia_is_never_persisted)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    GivenElectricalStepsSucceed();

    auto sm = CreateRecordingStateMachine();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
            {
                cb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 1.0e30f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _)).Times(0);

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code, state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachineSpeedIdentificationTest, a_failed_identification_restores_the_model_the_drive_had_before_it)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGainsAndInertia();
    GivenElectricalStepsSucceed();

    auto sm = CreateRecordingStateMachine();
    auto& cascade = sm.GetController();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
            {
                cb(std::nullopt, std::nullopt);
            }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_NEAR(cascade.lastMechanical.inertia.Value(), 0.01f, 1e-6f);
    EXPECT_NEAR(cascade.lastMechanical.viscousFriction.Value(), 0.005f, 1e-6f);
    EXPECT_NEAR(cascade.lastElectrical.resistance.Value(), 0.5f, 1e-6f);
}

TEST_F(FocStateMachineSpeedIdentificationTest, an_emergency_stop_during_identification_restores_the_previous_model)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGainsAndInertia();
    GivenElectricalStepsSucceed();

    auto sm = CreateRecordingStateMachine();
    auto& cascade = sm.GetController();

    // The service only reports itself running once the step has actually started it; entering Calibrating
    // is guarded on no async work being in flight, so it must not claim to be running before that.
    bool identificationInFlight = false;

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([&identificationInFlight](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto&)
            {
                identificationInFlight = true;
            }));
    EXPECT_CALL(mechIdentMock, IsRunning()).WillRepeatedly(Invoke([&identificationInFlight]()
        {
            return identificationInFlight;
        }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(identificationInFlight);

    identificationInFlight = false;
    EXPECT_EQ(sm.CmdEmergencyStop(), state_machine::CommandResult::ok);

    EXPECT_NEAR(cascade.lastMechanical.inertia.Value(), 0.01f, 1e-6f);
    EXPECT_NEAR(cascade.lastElectrical.resistance.Value(), 0.5f, 1e-6f);
}

TEST_F(FocStateMachineSpeedIdentificationTest, a_refused_electrical_model_still_restores_the_previous_one)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGainsAndInertia();
    GivenElectricalStepsSucceed();

    auto sm = CreateRefusingStateMachine();
    auto& cascade = sm.GetController();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _)).Times(0);
    EXPECT_CALL(nvmMock, SaveCalibration(_, _)).Times(0);

    cascade.refuseNextConfigure = true;
    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_GE(cascade.electricalConfigurations, 2u);
    EXPECT_NEAR(cascade.lastElectrical.resistance.Value(), 0.5f, 1e-6f);
    // The identified resistance happens to equal the stored one, so the value alone cannot show the
    // restore ran. Boot configures the mechanics once; a second configuration can only come from the
    // restore, since the refused electrical model stops the step before the provisional plant is applied.
    EXPECT_GE(cascade.mechanicalConfigurations, 2u);
    EXPECT_NEAR(cascade.lastMechanical.inertia.Value(), 0.01f, 1e-6f);
}
