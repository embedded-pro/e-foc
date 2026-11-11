#include "TestFocStateMachineHelper.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include <limits>
#include <memory>
#include <vector>

namespace
{
    using namespace testing;

    class FocStateMachineSpeedTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        using SpeedStateMachine = application::SpeedStateMachine;

        StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        infra::Execute setupStreamExpectations{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(_, _)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Available()).Times(AnyNumber()).WillRepeatedly(Return(1000));
                EXPECT_CALL(streamWriterMock, ConstructSaveMarker()).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, GetProcessedBytesSince(_)).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, SaveState(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(streamWriterMock, RestoreState(_)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Overwrite(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
            } };

        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        application::NvmActivity nvmActivity;
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
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([](services::CalibrationData& out, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = services::CalibrationData{};
                        onDone(services::NvmStatus::InvalidData);
                    }));
        }

        void GivenNvmValidWithSpeedGains()
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.speedLoopBandwidth = 50.0f;
            data.inertia = 0.005f;
            data.frictionViscous = 0.0f;
            data.stage = services::CalibrationStage::complete;

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
            bool nvmSaveOk = true,
            bool reachesSave = true)
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
                    .Times(mechOk ? 1 : 3)
                    .WillRepeatedly(Invoke([](std::size_t, const auto&,
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
                    .Times(3)
                    .WillRepeatedly(Invoke([](const foc::NewtonMeter&,
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

            if (!reachesSave)
                return;

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
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), foc::Weber{ 0.007f } },
                faultNotifierMock,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock },
                nvmActivity
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
            sm.Commissioning().CmdReAlign([](state_machine::CommandResult) {});
        }

        void GivenNvmValidWithSpeedGainsAndInertia()
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.speedLoopBandwidth = 50.0f;
            data.inertia = 0.01f;
            data.frictionViscous = 0.005f;
            data.stage = services::CalibrationStage::complete;

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
            data.speedLoopBandwidth = 50.0f;
            data.stage = services::CalibrationStage::complete;

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
            data.speedLoopBandwidth = 50.0f;
            data.inertia = inertia;
            data.frictionViscous = frictionViscous;
            data.stage = services::CalibrationStage::complete;

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
            return data;
        }
    };
}

TEST_F(FocStateMachineSpeedTest, nvm_invalid_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, nvm_valid_on_boot_remains_in_idle_pending_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, nvm_load_failure_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([](services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::HardwareFault);
            }));
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, calibration_calls_mech_ident_after_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, calibration_populates_inertia_and_velocity_gains)
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

TEST_F(FocStateMachineSpeedTest, calibrate_from_ready_re_runs_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, calibrate_from_enabled_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, alignment_nullopt_enters_fault)
{
    EXPECT_CALL(alignmentMock, LastFailure()).WillRepeatedly(Return(services::MotorAlignment::Failure::none));
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, mech_ident_failure_enters_fault)
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

TEST_F(FocStateMachineSpeedTest, nvm_save_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, true, true, false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, enable_from_ready_enters_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, disable_from_enabled_returns_to_ready)
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

TEST_F(FocStateMachineSpeedTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, disable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, fault_from_enabled_enters_fault)
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

TEST_F(FocStateMachineSpeedTest, fault_from_idle_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, fault_code_is_recorded)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachineSpeedTest, clear_fault_from_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, clear_fault_from_fault_with_valid_calibration_returns_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, clear_fault_from_non_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, clear_cal_from_ready_returns_to_idle)
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

TEST_F(FocStateMachineSpeedTest, clear_cal_from_enabled_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, late_pole_pairs_callback_after_fault_is_ignored)
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

TEST_F(FocStateMachineSpeedTest, late_alignment_callback_after_fault_is_ignored)
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

TEST_F(FocStateMachineSpeedTest, late_mech_ident_callback_after_fault_is_ignored)
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

TEST_F(FocStateMachineSpeedTest, clear_cal_during_calibrating_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, clear_cal_nvm_failure_enters_fault)
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

TEST_F(FocStateMachineSpeedTest, late_resistance_callback_after_fault_is_ignored)
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

TEST_F(FocStateMachineSpeedTest, late_nvm_save_callback_after_fault_is_ignored)
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

TEST_F(FocStateMachineSpeedTest, calibrate_is_rejected_until_the_boot_check_has_completed)
{
    GivenFaultNotifierRegistered();
    infra::Function<void(services::NvmStatus)> bootCb;
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([&bootCb](services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
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

    bootCb(services::NvmStatus::InvalidData);
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, enable_is_rejected_while_a_clear_calibration_is_outstanding)
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

TEST_F(FocStateMachineSpeedTest, clear_cal_invalidated_after_a_fault_drops_the_record_so_clear_fault_returns_to_idle)
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

TEST_F(FocStateMachineSpeedTest, clear_cal_invalidate_failure_callback_after_fault_does_not_re_enter_fault)
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

TEST_F(FocStateMachineSpeedTest, a_model_is_refused_while_the_drive_runs)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);
    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    EXPECT_FALSE(sm.ApplyModel(state_machine::MotorModel{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, foc::NewtonMeterSecondSquared{ 2.0e-5f }, foc::NewtonMeterSecondPerRadian{ 1.0e-5f } }));
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, a_model_is_applied_while_the_drive_is_stopped)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_TRUE(sm.ApplyModel(state_machine::MotorModel{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, foc::NewtonMeterSecondSquared{ 2.0e-5f }, foc::NewtonMeterSecondPerRadian{ 1.0e-5f } }));
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, a_model_with_a_flux_linkage_retunes_the_loops_to_it_and_the_record_keeps_its_own)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);
    const auto record = sm.Calibration().EffectiveFluxLinkage();
    auto model = state_machine::MotorModel{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, foc::NewtonMeterSecondSquared{ 2.0e-5f }, foc::NewtonMeterSecondPerRadian{ 1.0e-5f }, foc::Weber{ 0.9f * record.Value() } };

    ASSERT_TRUE(sm.ApplyModel(model));

    EXPECT_NEAR(sm.Calibration().ActiveFluxLinkage().Value(), 0.9f * record.Value(), 1e-9f);
    EXPECT_NEAR(sm.Calibration().EffectiveFluxLinkage().Value(), record.Value(), 1e-9f);
}

TEST_F(FocStateMachineSpeedTest, a_model_without_a_flux_linkage_keeps_the_record_s)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);
    const auto record = sm.Calibration().EffectiveFluxLinkage();

    ASSERT_TRUE(sm.ApplyModel(state_machine::MotorModel{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, foc::NewtonMeterSecondSquared{ 2.0e-5f }, foc::NewtonMeterSecondPerRadian{ 1.0e-5f } }));

    EXPECT_NEAR(sm.Calibration().ActiveFluxLinkage().Value(), record.Value(), 1e-9f);
}

TEST_F(FocStateMachineSpeedTest, the_online_estimates_start_from_the_record_and_have_not_settled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    const auto estimates = sm.CurrentOnlineEstimates();

    ASSERT_TRUE(estimates.has_value());
    EXPECT_NEAR(estimates->fluxLinkage.Value(), sm.Calibration().EffectiveFluxLinkage().Value(), 1e-9f);
    EXPECT_FALSE(estimates->fluxLinkageSettled);
}

namespace
{
    class FocStateMachineSpeedAutoTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        using SpeedAutoStateMachine = application::SpeedStateMachine;

        StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        infra::Execute setupStreamExpectations{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(_, _)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Available()).Times(AnyNumber()).WillRepeatedly(Return(1000));
                EXPECT_CALL(streamWriterMock, ConstructSaveMarker()).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, GetProcessedBytesSince(_)).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, SaveState(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(streamWriterMock, RestoreState(_)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Overwrite(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
            } };

        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        application::NvmActivity nvmActivity;
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
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([](services::CalibrationData& out, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = services::CalibrationData{};
                        onDone(services::NvmStatus::InvalidData);
                    }));
        }

        void GivenNvmValidWithSpeedGains()
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.speedLoopBandwidth = 50.0f;
            data.inertia = 0.005f;
            data.frictionViscous = 0.0f;
            data.stage = services::CalibrationStage::complete;

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
                    .Times(mechOk ? 1 : 3)
                    .WillRepeatedly(Invoke([](std::size_t, const auto&,
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
                    .Times(3)
                    .WillRepeatedly(Invoke([](const foc::NewtonMeter&,
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
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), foc::Weber{ 0.007f } },
                faultNotifierMock,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock },
                nvmActivity
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
            sm.Commissioning().CmdReAlign([](state_machine::CommandResult) {});
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
    EXPECT_CALL(alignmentMock, LastFailure()).WillRepeatedly(Return(services::MotorAlignment::Failure::none));
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
    infra::Function<void(services::NvmStatus)> bootCb;
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([&bootCb](services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
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

    bootCb(services::NvmStatus::InvalidData);
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

TEST_F(FocStateMachineSpeedTest, calibrate_from_calibrating_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, calibrate_from_fault_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, enable_from_calibrating_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, enable_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, enable_from_enabled_does_not_call_start_again)
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

TEST_F(FocStateMachineSpeedTest, disable_from_calibrating_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, disable_from_fault_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, clear_fault_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, clear_fault_from_calibrating_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, clear_fault_from_enabled_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, clear_cal_from_fault_is_rejected)
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

TEST_F(FocStateMachineSpeedTest, nvm_zero_resistance_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithZeroResistance();
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedTest, nvm_zero_inertia_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.0f, 0.01f);
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedTest, nvm_non_finite_inertia_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(std::numeric_limits<float>::quiet_NaN(), 0.01f);
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedTest, nvm_negative_friction_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.005f, -0.01f);
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedTest, nvm_non_finite_friction_fails_validation_and_stays_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.005f, std::numeric_limits<float>::infinity());
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedTest, nvm_zero_friction_is_valid_and_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.005f, 0.0f);
    auto sm = CreateSpeedStateMachine();
    AlignAfterBoot(sm);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, external_calibration_without_mechanics_stays_idle_with_partial_record)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    ExpectExternalCalibrationPersistsStage(services::CalibrationStage::none);

    EXPECT_EQ(sm.Commissioning().CmdReserveExternalCalibration(), state_machine::CommandResult::ok);

    auto result = state_machine::CommandResult::rejected;
    sm.Commissioning().CmdCompleteExternalCalibration(ElectricalOnlyData(), [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_TRUE(sm.HasPartialCalibration());
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedTest, entering_the_inertia_completes_an_electrical_only_record)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    ExpectExternalCalibrationPersistsStage(services::CalibrationStage::none);
    ASSERT_EQ(sm.Commissioning().CmdReserveExternalCalibration(), state_machine::CommandResult::ok);
    sm.Commissioning().CmdCompleteExternalCalibration(ElectricalOnlyData(), [](state_machine::CommandResult) {});
    ASSERT_TRUE(sm.HasPartialCalibration());

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(data.inertia, 1.0e-5f, 1e-10f);
                EXPECT_EQ(data.stage, services::CalibrationStage::complete);
                onDone(services::NvmStatus::Ok);
            }));

    auto result = state_machine::CommandResult::rejected;
    sm.Commissioning().CmdSetMotorParameter(services::MotorParameter::inertia, 1.0e-5f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::ok);
    EXPECT_FALSE(sm.HasPartialCalibration());
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));

    AlignAfterBoot(sm);
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, an_electrical_identification_keeps_the_mechanics_of_the_record)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.005f, 0.001f);
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(data.inertia, 0.005f, 1e-8f);
                EXPECT_NEAR(data.frictionViscous, 0.001f, 1e-8f);
                EXPECT_NEAR(data.speedLoopBandwidth, 50.0f, 1e-4f);
                EXPECT_EQ(data.stage, services::CalibrationStage::complete);
                onDone(services::NvmStatus::Ok);
            }));

    ASSERT_EQ(sm.Commissioning().CmdReserveExternalCalibration(), state_machine::CommandResult::ok);
    sm.Commissioning().CmdCompleteExternalCalibration(ElectricalOnlyData(), [](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedTest, a_motor_parameter_that_cannot_be_stored_leaves_the_record_unchanged)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.005f, 0.001f);
    auto sm = CreateSpeedStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::WriteFailed);
            }));

    auto result = state_machine::CommandResult::ok;
    sm.Commissioning().CmdSetMotorParameter(services::MotorParameter::inertia, 0.02f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::nvmFailed);
    EXPECT_NEAR(sm.Calibration().Data().inertia, 0.005f, 1e-8f);
}

TEST_F(FocStateMachineSpeedTest, a_non_physical_motor_parameter_is_rejected_without_a_write)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithMechanics(0.005f, 0.001f);
    auto sm = CreateSpeedStateMachine();

    auto result = state_machine::CommandResult::ok;
    sm.Commissioning().CmdSetMotorParameter(services::MotorParameter::inertia, 0.0f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });
    EXPECT_EQ(result, state_machine::CommandResult::rejected);

    sm.Commissioning().CmdSetMotorParameter(services::MotorParameter::polePairs, 4.5f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });
    EXPECT_EQ(result, state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedTest, complete_external_calibration_without_reserve_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    auto result = state_machine::CommandResult::ok;
    sm.Commissioning().CmdCompleteExternalCalibration(ElectricalOnlyData(), [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::rejected);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPartialCalibration());
}

TEST_F(FocStateMachineSpeedTest, reserve_external_calibration_is_rejected_while_calibrating)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();

    EXPECT_EQ(sm.Commissioning().CmdReserveExternalCalibration(), state_machine::CommandResult::ok);
    EXPECT_EQ(sm.Commissioning().CmdReserveExternalCalibration(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineSpeedTest, has_pending_async_work_true_when_mech_ident_is_running)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateSpeedStateMachine();
    state_machine::FocStateMachineBase& base = sm;

    EXPECT_CALL(mechIdentMock, IsRunning()).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_TRUE(base.HasPendingAsyncWork());
}

TEST_F(FocStateMachineSpeedTest, has_pending_async_work_true_when_electrical_ident_is_running)
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
        : public FocStateMachineSpeedTest
    {
    public:
        using RecordingStateMachine = application::OuterLoopStateMachineFor<RecordingSpeedController, RecordingSpeedController>;

        foc::Weber fluxLinkage{ 0.007f };

        using RefusingStateMachine = application::OuterLoopStateMachineFor<RefusingSpeedController, RefusingSpeedController>;

        RefusingStateMachine CreateRefusingStateMachine()
        {
            return RefusingStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), fluxLinkage },
                faultNotifierMock,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock },
                nvmActivity
            };
        }

        RecordingStateMachine CreateRecordingStateMachine()
        {
            return RecordingStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), fluxLinkage },
                faultNotifierMock,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock },
                nvmActivity
            };
        }

        RecordingStateMachine CreateStateMachineRequiringAgreement()
        {
            return RecordingStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), fluxLinkage, 2 },
                faultNotifierMock,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock },
                nvmActivity
            };
        }

        void GivenIdentificationRuns(std::initializer_list<float> inertias)
        {
            const std::vector<float> values{ inertias };
            auto next = std::make_shared<std::size_t>(0);

            EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
                .Times(static_cast<int>(values.size()))
                .WillRepeatedly(Invoke([values, next](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
                    {
                        cb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ values[(*next)++] });
                    }));
        }

        void GivenElectricalStepsSucceed(std::size_t alignments = 1)
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
                .Times(alignments)
                .WillRepeatedly(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
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
    GivenElectricalStepsSucceed(3);

    auto sm = CreateRecordingStateMachine();
    auto& cascade = sm.GetController();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .Times(3)
        .WillOnce(Invoke([&observed, &cascade](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
            {
                observed.electricalConfigurations = cascade.electricalConfigurations;
                observed.mechanicalConfigurations = cascade.mechanicalConfigurations;
                observed.resistance = cascade.lastElectrical.resistance.Value();
                observed.provisionalInertia = cascade.lastMechanical.inertia.Value();
                cb(std::nullopt, std::nullopt);
            }))
        .WillRepeatedly(Invoke([](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
            {
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
    GivenElectricalStepsSucceed(3);

    auto sm = CreateRecordingStateMachine();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .Times(3)
        .WillRepeatedly(Invoke([&observed](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config& config, const auto& cb)
            {
                observed = config;
                cb(std::nullopt, std::nullopt);
            }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_GT(observed.maxCurrent.Value(), 10.0f);
    EXPECT_NEAR(observed.maxCurrent.Value(), 10.0f * services::identificationCurrentMargin, 1e-4f);
    EXPECT_GT(observed.maxSpeed.Value(), observed.targetSpeed.Value());
    EXPECT_LT(observed.dwellSpeed.Value(), observed.targetSpeed.Value());
    EXPECT_TRUE(services::IsUsableIdentificationConfig(observed));
}

TEST_F(FocStateMachineSpeedIdentificationTest, identification_never_runs_when_the_speed_loop_cannot_be_configured)
{
    fluxLinkage = foc::Weber{ 0.0f };

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
    GivenElectricalStepsSucceed(3);

    auto sm = CreateRecordingStateMachine();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .Times(3)
        .WillRepeatedly(Invoke([](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
            {
                cb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 1.0e30f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _)).Times(0);

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code, state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachineSpeedIdentificationTest, an_identification_that_fails_once_is_run_again_from_a_new_alignment_and_the_calibration_completes)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    GivenElectricalStepsSucceed(2);

    auto sm = CreateRecordingStateMachine();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
            {
                cb(std::nullopt, std::nullopt);
            }))
        .WillOnce(Invoke([](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
            {
                cb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 0.005f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedIdentificationTest, an_identification_that_keeps_failing_is_given_up_after_three_attempts)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    GivenElectricalStepsSucceed(3);

    auto sm = CreateRecordingStateMachine();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .Times(3)
        .WillRepeatedly(Invoke([](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
            {
                cb(std::nullopt, std::nullopt);
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _)).Times(0);

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code, state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachineSpeedIdentificationTest, two_identification_runs_that_agree_are_saved_as_their_mean)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    GivenElectricalStepsSucceed(2);
    GivenIdentificationRuns({ 0.0050f, 0.0055f });

    auto sm = CreateStateMachineRequiringAgreement();
    float savedInertia = 0.0f;

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&savedInertia](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                savedInertia = data.inertia;
                onDone(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_NEAR(savedInertia, 0.00525f, 1e-6f);
}

TEST_F(FocStateMachineSpeedIdentificationTest, an_outlying_run_is_outvoted_by_two_runs_that_agree)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    GivenElectricalStepsSucceed(3);
    GivenIdentificationRuns({ 0.0005f, 0.0050f, 0.0052f });

    auto sm = CreateStateMachineRequiringAgreement();
    float savedInertia = 0.0f;

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&savedInertia](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                savedInertia = data.inertia;
                onDone(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_NEAR(savedInertia, 0.0051f, 1e-6f);
}

TEST_F(FocStateMachineSpeedIdentificationTest, three_identification_runs_that_never_agree_fail_the_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    GivenElectricalStepsSucceed(3);
    GivenIdentificationRuns({ 0.0010f, 0.0050f, 0.0200f });

    auto sm = CreateStateMachineRequiringAgreement();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _)).Times(0);

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code, state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachineSpeedIdentificationTest, a_failed_identification_restores_the_model_the_drive_had_before_it)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGainsAndInertia();
    GivenElectricalStepsSucceed(3);

    auto sm = CreateRecordingStateMachine();
    auto& cascade = sm.GetController();

    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .Times(3)
        .WillRepeatedly(Invoke([](const foc::NewtonMeter&, std::size_t, const services::MechanicalParametersIdentification::Config&, const auto& cb)
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

namespace
{
    class FocStateMachineSpeedCoggingTest
        : public FocStateMachineSpeedTest
    {
    public:
        StrictMock<services::CoggingMappingMock> coggingMock;
        infra::Execute setupCoggingExpectations{ [this]()
            {
                EXPECT_CALL(coggingMock, Abort()).Times(AnyNumber());
            } };
        bool mappingInFlight{ false };

        SpeedStateMachine CreateStateMachineMappingCogging()
        {
            EXPECT_CALL(coggingMock, IsRunning()).WillRepeatedly(Invoke([this]()
                {
                    return mappingInFlight;
                }));

            auto services = application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), foc::Weber{ 0.007f } };
            services.coggingMappingOverride = std::ref(coggingMock);

            return SpeedStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                services,
                faultNotifierMock,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock },
                nvmActivity
            };
        }

        static services::CoggingMap AMap()
        {
            services::CoggingMap map{};
            map.torque[3] = 127;
            map.torque[9] = -127;
            map.torqueScale = 3.0e-5f;
            map.peak = foc::NewtonMeter{ 3.8e-3f };
            return map;
        }
    };
}

TEST_F(FocStateMachineSpeedCoggingTest, calibration_maps_the_cogging_with_the_agreed_mechanics_and_stores_the_map)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateStateMachineMappingCogging();

    EXPECT_CALL(coggingMock, Measure(_, _, _, 4, _, _))
        .WillOnce(Invoke([](foc::NewtonMeter torqueConstant, foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian viscousFriction, std::size_t, const services::CoggingMapping::Config&, const infra::Function<void(const services::CoggingMeasurement&)>& onDone)
            {
                EXPECT_GT(torqueConstant.Value(), 0.0f);
                EXPECT_NEAR(inertia.Value(), 0.005f, 1e-6f);
                EXPECT_NEAR(viscousFriction.Value(), 0.01f, 1e-6f);
                onDone(services::CoggingMeasurement{ AMap(), foc::NewtonMeter{ 4.0e-3f } });
            }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    const auto& data = std::get<state_machine::Ready>(sm.CurrentState()).loadedData;
    EXPECT_EQ(data.coggingTorque, AMap().torque);
    EXPECT_NEAR(data.coggingTorqueScale, AMap().torqueScale, 1e-10f);
    EXPECT_NEAR(data.frictionCoulomb, 4.0e-3f, 1e-9f);
}

TEST_F(FocStateMachineSpeedCoggingTest, a_cogging_map_that_cannot_be_measured_leaves_the_calibration_complete_without_one)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateStateMachineMappingCogging();

    EXPECT_CALL(coggingMock, Measure(_, _, _, _, _, _))
        .WillOnce(Invoke([](foc::NewtonMeter, foc::NewtonMeterSecondSquared, foc::NewtonMeterSecondPerRadian, std::size_t, const services::CoggingMapping::Config&, const infra::Function<void(const services::CoggingMeasurement&)>& onDone)
            {
                onDone(services::CoggingMeasurement{});
            }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Ready>(sm.CurrentState()).loadedData.coggingTorqueScale, 0.0f);
    EXPECT_EQ(std::get<state_machine::Ready>(sm.CurrentState()).loadedData.frictionCoulomb, 0.0f);
}

TEST_F(FocStateMachineSpeedCoggingTest, an_emergency_stop_while_mapping_aborts_the_mapping)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(true, true, true, true, true, false);
    auto sm = CreateStateMachineMappingCogging();

    EXPECT_CALL(coggingMock, Measure(_, _, _, _, _, _))
        .WillOnce(Invoke([this](foc::NewtonMeter, foc::NewtonMeterSecondSquared, foc::NewtonMeterSecondPerRadian, std::size_t, const services::CoggingMapping::Config&, const infra::Function<void(const services::CoggingMeasurement&)>&)
            {
                mappingInFlight = true;
            }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(mappingInFlight);

    EXPECT_CALL(coggingMock, Abort()).Times(AtLeast(1)).WillRepeatedly(Invoke([this]()
        {
            mappingInFlight = false;
        }));
    EXPECT_EQ(sm.CmdEmergencyStop(), state_machine::CommandResult::ok);

    EXPECT_FALSE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

namespace
{
    class FrictionRecordingSpeedCascade
        : public foc::SpeedCascade
    {
    public:
        using foc::SpeedCascade::SpeedCascade;

        void LoadFrictionFeedForward(foc::NewtonMeter coulomb, foc::NewtonMeter torqueConstant) override
        {
            ++loads;
            lastCoulomb = coulomb.Value();
            lastTorqueConstant = torqueConstant.Value();
            foc::SpeedCascade::LoadFrictionFeedForward(coulomb, torqueConstant);
        }

        void ClearFrictionFeedForward() override
        {
            ++clears;
            foc::SpeedCascade::ClearFrictionFeedForward();
        }

        std::size_t loads{ 0 };
        std::size_t clears{ 0 };
        float lastCoulomb{ 0.0f };
        float lastTorqueConstant{ 0.0f };
    };

    using FrictionRecordingController = foc::FocController<FrictionRecordingSpeedCascade>;

    class FocStateMachineSpeedFrictionTest
        : public FocStateMachineSpeedCoggingTest
    {
    public:
        using FrictionStateMachine = application::OuterLoopStateMachineFor<FrictionRecordingController, FrictionRecordingController>;

        FrictionStateMachine CreateFrictionStateMachine()
        {
            EXPECT_CALL(coggingMock, IsRunning()).WillRepeatedly(Return(false));

            auto services = application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), foc::Weber{ 0.007f } };
            services.coggingMappingOverride = std::ref(coggingMock);

            return FrictionStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                services,
                faultNotifierMock,
                application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock },
                nvmActivity
            };
        }

        void GivenNvmValidWithFriction(float frictionCoulomb)
        {
            services::CalibrationData data{};
            data.polePairs = 4;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.fluxLinkage = 0.007f;
            data.speedLoopBandwidth = 50.0f;
            data.inertia = 0.01f;
            data.frictionViscous = 0.005f;
            data.frictionCoulomb = frictionCoulomb;
            data.stage = services::CalibrationStage::complete;

            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([data](services::CalibrationData& out, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = data;
                        onDone(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
        }

        void ExpectImplausibleFrictionStoredAsNone(float measured)
        {
            GivenFaultNotifierRegistered();
            GivenNvmInvalid();
            ExpectSpeedCalibrationSequence();
            auto sm = CreateFrictionStateMachine();
            GivenCoggingMeasures(services::CoggingMeasurement{ std::nullopt, foc::NewtonMeter{ measured } });

            sm.CmdCalibrate([](state_machine::CommandResult) {});

            ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
            EXPECT_EQ(std::get<state_machine::Ready>(sm.CurrentState()).loadedData.frictionCoulomb, 0.0f);
            EXPECT_EQ(sm.GetController().loads, 0u);
        }

        void GivenCoggingMeasures(services::CoggingMeasurement measurement, FrictionRecordingController** cascade = nullptr)
        {
            EXPECT_CALL(coggingMock, Measure(_, _, _, _, _, _))
                .WillOnce(Invoke([measurement, cascade](foc::NewtonMeter, foc::NewtonMeterSecondSquared, foc::NewtonMeterSecondPerRadian, std::size_t, const services::CoggingMapping::Config&, const infra::Function<void(const services::CoggingMeasurement&)>& onDone)
                    {
                        if (cascade != nullptr)
                        {
                            EXPECT_EQ((*cascade)->loads, 0u);
                            EXPECT_GE((*cascade)->clears, 1u);
                        }
                        onDone(measurement);
                    }));
        }
    };
}

TEST_F(FocStateMachineSpeedFrictionTest, a_stored_dry_friction_is_loaded_into_the_cascade_with_the_torque_constant)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithFriction(4.0e-3f);

    auto sm = CreateFrictionStateMachine();

    EXPECT_EQ(sm.GetController().loads, 1u);
    EXPECT_NEAR(sm.GetController().lastCoulomb, 4.0e-3f, 1e-9f);
    EXPECT_NEAR(sm.GetController().lastTorqueConstant, 1.5f * 4.0f * 0.007f, 1e-6f);
}

TEST_F(FocStateMachineSpeedFrictionTest, a_record_without_a_dry_friction_leaves_the_cascade_without_a_feed_forward)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithFriction(0.0f);

    auto sm = CreateFrictionStateMachine();

    EXPECT_EQ(sm.GetController().loads, 0u);
    EXPECT_GE(sm.GetController().clears, 1u);
}

TEST_F(FocStateMachineSpeedFrictionTest, a_calibration_stores_the_measured_dry_friction_and_applies_it_after_both_measurements)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateFrictionStateMachine();
    auto* cascade = &sm.GetController();
    GivenCoggingMeasures(services::CoggingMeasurement{ std::nullopt, foc::NewtonMeter{ 3.7e-3f } }, &cascade);

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_NEAR(std::get<state_machine::Ready>(sm.CurrentState()).loadedData.frictionCoulomb, 3.7e-3f, 1e-9f);
    EXPECT_EQ(cascade->loads, 1u);
    EXPECT_NEAR(cascade->lastCoulomb, 3.7e-3f, 1e-9f);
}

TEST_F(FocStateMachineSpeedFrictionTest, a_negative_measured_dry_friction_is_stored_as_none)
{
    ExpectImplausibleFrictionStoredAsNone(-1.0e-3f);
}

TEST_F(FocStateMachineSpeedFrictionTest, a_not_a_number_measured_dry_friction_is_stored_as_none)
{
    ExpectImplausibleFrictionStoredAsNone(std::numeric_limits<float>::quiet_NaN());
}

TEST_F(FocStateMachineSpeedFrictionTest, an_infinite_measured_dry_friction_is_stored_as_none)
{
    ExpectImplausibleFrictionStoredAsNone(std::numeric_limits<float>::infinity());
}

TEST_F(FocStateMachineSpeedFrictionTest, writing_the_dry_friction_reloads_the_cascade_feed_forward)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithFriction(4.0e-3f);
    auto sm = CreateFrictionStateMachine();
    ASSERT_EQ(sm.GetController().loads, 1u);

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(data.frictionCoulomb, 6.0e-3f, 1e-9f);
                onDone(services::NvmStatus::Ok);
            }));

    auto result = state_machine::CommandResult::rejected;
    sm.Commissioning().CmdSetMotorParameter(services::MotorParameter::frictionCoulomb, 6.0e-3f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::ok);
    EXPECT_EQ(sm.GetController().loads, 2u);
    EXPECT_NEAR(sm.GetController().lastCoulomb, 6.0e-3f, 1e-9f);
    EXPECT_NEAR(sm.Calibration().Data().frictionViscous, 0.005f, 1e-9f);
}

TEST_F(FocStateMachineSpeedFrictionTest, writing_a_zero_dry_friction_clears_the_cascade_feed_forward)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithFriction(4.0e-3f);
    auto sm = CreateFrictionStateMachine();
    const auto clears = sm.GetController().clears;

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));

    sm.Commissioning().CmdSetMotorParameter(services::MotorParameter::frictionCoulomb, 0.0f, [](state_machine::CommandResult) {});

    EXPECT_EQ(sm.GetController().loads, 1u);
    EXPECT_EQ(sm.GetController().clears, clears + 1);
}

TEST_F(FocStateMachineSpeedFrictionTest, a_dry_friction_that_cannot_be_stored_leaves_the_feed_forward_alone)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithFriction(4.0e-3f);
    auto sm = CreateFrictionStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::WriteFailed);
            }));

    sm.Commissioning().CmdSetMotorParameter(services::MotorParameter::frictionCoulomb, 6.0e-3f, [](state_machine::CommandResult) {});

    EXPECT_EQ(sm.GetController().loads, 1u);
    EXPECT_NEAR(sm.Calibration().Data().frictionCoulomb, 4.0e-3f, 1e-9f);
}

TEST_F(FocStateMachineSpeedTest, the_zero_vector_slopes_of_each_control_period_reach_the_power_stage_sink)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    StrictMock<foc::ZeroVectorSlopeSinkMock> sink;
    SpeedStateMachine sm{ tracer, application::MotorHardware{ inverterMock, encoderMock, vdc, 0, nullptr, &sink }, nvmMock, application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), foc::Weber{ 0.007f } }, faultNotifierMock, application::OuterLoopArgs{ foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock }, nvmActivity };
    AlignAfterBoot(sm);
    EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).WillRepeatedly([this](hal::Hertz, const infra::Function<void(foc::PhaseCurrents)>& onDone)
        {
            inverterMock.StorePhaseCurrentsCallback(onDone);
        });
    EXPECT_CALL(inverterMock, Start());
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    EXPECT_CALL(lowPriorityInterruptMock, Trigger()).Times(AnyNumber());
    EXPECT_CALL(encoderMock, Read()).WillOnce(Return(foc::Radians{ 0.0f }));
    EXPECT_CALL(inverterMock, ThreePhasePwmOutput(_));
    EXPECT_CALL(sink, ZeroVectorSlopes(_));
    inverterMock.TriggerPhaseCurrentsCallback(foc::PhaseCurrents{ foc::Ampere{ 0.0f }, foc::Ampere{ 0.5f }, foc::Ampere{ -0.5f } });
}
