#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/foc/implementations/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/cli/TerminalPosition.hpp"
#include "core/services/cli/TerminalSpeed.hpp"
#include "core/services/cli/TerminalTorque.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/FocStateMachineImpl.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <gmock/gmock.h>

namespace
{
    using namespace testing;

    class StreamWriterMock
        : public infra::StreamWriter
    {
    public:
        using StreamWriter::StreamWriter;

        MOCK_METHOD2(Insert, void(infra::ConstByteRange range, infra::StreamErrorPolicy& errorPolicy));
        MOCK_CONST_METHOD0(Available, std::size_t());
        MOCK_CONST_METHOD0(ConstructSaveMarker, std::size_t());
        MOCK_CONST_METHOD1(GetProcessedBytesSince, std::size_t(std::size_t marker));
        MOCK_METHOD1(SaveState, infra::ByteRange(std::size_t marker));
        MOCK_METHOD1(RestoreState, void(infra::ByteRange range));
        MOCK_METHOD1(Overwrite, infra::ByteRange(std::size_t marker));
    };

    class FocStateMachineTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using TestedStateMachine = application::FocStateMachineImpl<foc::FocTorqueImpl, services::TerminalFocTorqueInteractor>;

        StrictMock<StreamWriterMock> streamWriterMock;
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

TEST_F(FocStateMachineTest, nvm_invalid_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, nvm_valid_on_boot_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, nvm_load_failure_on_boot_remains_in_idle)
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

TEST_F(FocStateMachineTest, calibrate_from_idle_runs_full_sequence_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, calibrate_from_ready_re_runs_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, calibrate_records_pole_pairs_in_ready_state)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    const auto& ready = std::get<state_machine::Ready>(sm.CurrentState());
    EXPECT_EQ(ready.loadedData.polePairs, uint8_t{ 7 });
}

TEST_F(FocStateMachineTest, calibrate_records_resistance_in_ready_state)
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

TEST_F(FocStateMachineTest, calibrate_from_enabled_is_rejected)
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

TEST_F(FocStateMachineTest, pole_pairs_nullopt_enters_fault)
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

TEST_F(FocStateMachineTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, alignment_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, nvm_save_failure_after_full_calibration_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- Enable / Disable ---

TEST_F(FocStateMachineTest, enable_from_ready_starts_foc_and_enters_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, disable_from_enabled_stops_foc_and_enters_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, disable_from_ready_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Fault handling ---

TEST_F(FocStateMachineTest, fault_from_enabled_stops_pwm_and_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, fault_from_idle_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overvoltage);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, fault_from_ready_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::encoderLoss);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, fault_code_is_recorded)
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

TEST_F(FocStateMachineTest, clear_fault_from_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTest, clear_fault_from_enabled_is_rejected)
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

TEST_F(FocStateMachineTest, clear_cal_from_ready_invalidates_nvm_and_returns_to_idle)
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

TEST_F(FocStateMachineTest, clear_cal_from_idle_calls_invalidate)
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

TEST_F(FocStateMachineTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

// --- Auto-mode transition policy ---

namespace
{
    class FocStateMachineAutoModeTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using AutoStateMachine = application::FocStateMachineImpl<foc::FocTorqueImpl,
            services::TerminalFocTorqueInteractor,
            state_machine::AutoTransitionPolicy>;

        StrictMock<StreamWriterMock> streamWriterMock;
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

        AutoStateMachine CreateAutoStateMachine()
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

TEST_F(FocStateMachineAutoModeTest, auto_mode_starts_in_idle_when_nvm_invalid)
{
    EXPECT_CALL(faultNotifierMock, Register(_))
        .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
            {
                faultNotifierMock.StoreHandler(handler);
            }));
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(false);
            }));

    auto sm = CreateAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineAutoModeTest, auto_mode_calibrate_enable_disable_cycle)
{
    EXPECT_CALL(faultNotifierMock, Register(_))
        .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
            {
                faultNotifierMock.StoreHandler(handler);
            }));
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(false);
            }));
    auto sm = CreateAutoStateMachine();

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
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineAutoModeTest, auto_mode_fault_and_clear_cycle)
{
    EXPECT_CALL(faultNotifierMock, Register(_))
        .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
            {
                faultNotifierMock.StoreHandler(handler);
            }));
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(false);
            }));
    auto sm = CreateAutoStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdClearFault();
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Safety: fault during calibration stops inverter ---

TEST_F(FocStateMachineTest, fault_during_calibration_stops_inverter_and_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&)
            {
                // Callback deliberately not called — calibration remains pending
            }));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    // Fault must stop the inverter even while calibrating
    EXPECT_CALL(inverterMock, Stop()).Times(AtLeast(1));
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

// --- CLI safety: start command must not bypass state machine guards ---

TEST_F(FocStateMachineTest, cli_start_command_does_not_bypass_idle_guard)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    // Typing "start" must NOT call FocBase::Enable() -> FocController::Start() -> inverterMock.Start().
    // With StrictMock, any unexpected Start() call on inverterMock fails this test.
    communication.dataReceived(infra::MakeStringByteRange("start\r"));
    ExecuteAllActions();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

// --- Speed-mode fixture with mechanical identification ---

namespace
{
    class FocStateMachineSpeedModeTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using SpeedStateMachine = application::FocStateMachineImpl<
            foc::FocSpeedImpl,
            services::TerminalFocSpeedInteractor>;

        StrictMock<StreamWriterMock> streamWriterMock;
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

        void ExpectSpeedCalibrationSequence(bool mechOk = true)
        {
            EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                .WillOnce(Invoke([](const auto&,
                                     const infra::Function<void(std::optional<std::size_t>)>& cb)
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
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::Ok);
                        }));
                EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
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
            }
        }

        SpeedStateMachine CreateSpeedStateMachine()
        {
            return SpeedStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                inverterMock, encoderMock, vdc, nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
                faultNotifierMock,
                foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock
            };
        }
    };
}

TEST_F(FocStateMachineSpeedModeTest, speed_mode_calibration_calls_mech_ident_after_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence();
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedModeTest, speed_mode_mech_ident_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectSpeedCalibrationSequence(false);
    auto sm = CreateSpeedStateMachine();

    sm.CmdCalibrate();

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code,
        state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachineSpeedModeTest, speed_mode_calibration_populates_inertia_and_velocity_gains)
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

TEST_F(FocStateMachineSpeedModeTest, speed_mode_nvm_valid_on_boot_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithSpeedGains();
    auto sm = CreateSpeedStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Speed-mode with AutoTransitionPolicy ---

namespace
{
    class FocStateMachineSpeedAutoModeTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using SpeedAutoStateMachine = application::FocStateMachineImpl<
            foc::FocSpeedImpl,
            services::TerminalFocSpeedInteractor,
            state_machine::AutoTransitionPolicy>;

        StrictMock<StreamWriterMock> streamWriterMock;
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

        SpeedAutoStateMachine CreateSpeedAutoStateMachine()
        {
            return SpeedAutoStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                inverterMock, encoderMock, vdc, nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
                faultNotifierMock,
                foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock
            };
        }
    };
}

TEST_F(FocStateMachineSpeedAutoModeTest, speed_auto_mode_starts_in_idle_when_nvm_invalid)
{
    EXPECT_CALL(faultNotifierMock, Register(_))
        .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
            {
                faultNotifierMock.StoreHandler(handler);
            }));
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(false);
            }));

    auto sm = CreateSpeedAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineSpeedAutoModeTest, speed_auto_mode_calibrate_enable_disable_cycle)
{
    EXPECT_CALL(faultNotifierMock, Register(_))
        .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
            {
                faultNotifierMock.StoreHandler(handler);
            }));
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(false);
            }));
    auto sm = CreateSpeedAutoStateMachine();

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
    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([](const foc::NewtonMeter&, std::size_t,
                             const services::MechanicalParametersIdentification::Config&,
                             const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>,
                                 std::optional<foc::NewtonMeterSecondSquared>)>& cb)
            {
                cb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 0.005f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

// --- Position-mode fixture with CLI transition policy ---

namespace
{
    class FocStateMachinePositionModeTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using PositionStateMachine = application::FocStateMachineImpl<
            foc::FocPositionImpl,
            services::TerminalFocPositionInteractor>;

        StrictMock<StreamWriterMock> streamWriterMock;
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

        void ExpectPositionCalibrationSequence(bool mechOk = true)
        {
            EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                .WillOnce(Invoke([](const auto&,
                                     const infra::Function<void(std::optional<std::size_t>)>& cb)
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
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::Ok);
                        }));
                EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
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
            }
        }

        PositionStateMachine CreatePositionStateMachine()
        {
            return PositionStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                inverterMock, encoderMock, vdc, nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
                faultNotifierMock,
                foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock
            };
        }
    };
}

TEST_F(FocStateMachinePositionModeTest, position_mode_calibration_calls_mech_ident_after_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence();
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionModeTest, position_mode_mech_ident_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectPositionCalibrationSequence(false);
    auto sm = CreatePositionStateMachine();

    sm.CmdCalibrate();

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code,
        state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachinePositionModeTest, position_mode_no_mech_ident_override_enters_fault)
{
    EXPECT_CALL(faultNotifierMock, Register(_))
        .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
            {
                faultNotifierMock.StoreHandler(handler);
            }));
    GivenNvmInvalid();

    PositionStateMachine sm{
        application::TerminalAndTracer{ terminal, tracer },
        inverterMock, encoderMock, vdc, nvmMock,
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

TEST_F(FocStateMachinePositionModeTest, position_mode_calibration_populates_inertia_and_velocity_gains)
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

TEST_F(FocStateMachinePositionModeTest, position_mode_nvm_valid_on_boot_transitions_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValidWithPositionGains();
    auto sm = CreatePositionStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionModeTest, position_mode_enable_disable_cycle)
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

// --- Position-mode with AutoTransitionPolicy ---

namespace
{
    class FocStateMachinePositionAutoModeTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        using PositionAutoStateMachine = application::FocStateMachineImpl<
            foc::FocPositionImpl,
            services::TerminalFocPositionInteractor,
            state_machine::AutoTransitionPolicy>;

        StrictMock<StreamWriterMock> streamWriterMock;
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

        PositionAutoStateMachine CreatePositionAutoStateMachine()
        {
            return PositionAutoStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                inverterMock, encoderMock, vdc, nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
                faultNotifierMock,
                foc::Ampere{ 10.0f }, hal::Hertz{ 1000 }, lowPriorityInterruptMock
            };
        }
    };
}

TEST_F(FocStateMachinePositionAutoModeTest, position_auto_mode_starts_in_idle_when_nvm_invalid)
{
    EXPECT_CALL(faultNotifierMock, Register(_))
        .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
            {
                faultNotifierMock.StoreHandler(handler);
            }));
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(false);
            }));

    auto sm = CreatePositionAutoStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachinePositionAutoModeTest, position_auto_mode_calibrate_enable_disable_cycle)
{
    EXPECT_CALL(faultNotifierMock, Register(_))
        .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
            {
                faultNotifierMock.StoreHandler(handler);
            }));
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
            {
                onDone(false);
            }));
    auto sm = CreatePositionAutoStateMachine();

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
    EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
        .WillOnce(Invoke([](const foc::NewtonMeter&, std::size_t,
                             const services::MechanicalParametersIdentification::Config&,
                             const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>,
                                 std::optional<foc::NewtonMeterSecondSquared>)>& cb)
            {
                cb(foc::NewtonMeterSecondPerRadian{ 0.01f }, foc::NewtonMeterSecondSquared{ 0.005f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    sm.CmdCalibrate();
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}
