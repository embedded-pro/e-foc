#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/foc/implementations/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/FocMotorCanBridge.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <gtest/gtest.h>
#include <optional>

namespace
{
    using namespace testing;
    using namespace services;

    using TestedControlMode = state_machine::ControlModeStateMachine;

    using BridgeType = state_machine::FocMotorCanBridge;

    // Captures ACKs emitted by the bridge so the tests can assert on the
    // universal ACK reason without needing the full CanProtocolServer.
    struct AcknowledgerSpy
        : public services::CanCommandAcknowledger
    {
        struct Entry
        {
            uint8_t category{ 0 };
            uint8_t commandType{ 0 };
            services::CanAckStatus status{ services::CanAckStatus::success };
        };

        std::optional<Entry> last;
        std::size_t count{ 0 };

        void SendCommandAck(uint8_t category, uint8_t commandType, services::CanAckStatus status) override
        {
            last = Entry{ category, commandType, status };
            ++count;
        }

        void Reset()
        {
            last.reset();
            count = 0;
        }
    };

    class FocMotorCanBridgeTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy tracerStream{ streamWriterMock };
        services::TracerToStream tracer{ tracerStream };
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

        uint8_t lastSentMessageType{ 0 };
        bool selectModeResponseSent{ false };
        services::FocMotorMode lastSelectModeResponseActiveMode{ services::FocMotorMode::torque };
        bool categoryErrorSent{ false };
        uint8_t lastCategoryErrorOriginCmd{ 0 };
        services::FocMotorCategoryError lastCategoryErrorReason{ services::FocMotorCategoryError::busy };

        StrictMock<hal::CanMock> canMock;

        infra::Execute setupCanExpectations{ [this]()
            {
                EXPECT_CALL(canMock, SendData(_, _, _))
                    .Times(AnyNumber())
                    .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                        {
                            lastSentMessageType = services::ExtractCanMessageType(id.Get29BitId());
                            if (lastSentMessageType == services::focSelectControlModeResponseId && !msg.empty())
                            {
                                selectModeResponseSent = true;
                                lastSelectModeResponseActiveMode = static_cast<services::FocMotorMode>(msg[0]);
                            }
                            if (lastSentMessageType == services::focCategoryErrorResponseId && msg.size() >= 2)
                            {
                                categoryErrorSent = true;
                                lastCategoryErrorOriginCmd = msg[0];
                                lastCategoryErrorReason = static_cast<services::FocMotorCategoryError>(msg[1]);
                            }
                            cb(true);
                        }));
            } };

        services::CanFrameTransport canTransport{ canMock, 1 };
        services::FocMotorCategoryServer motorServer{ canTransport };
        AcknowledgerSpy ackSpy;

        std::optional<TestedControlMode> coordinator;
        std::optional<BridgeType> bridge;

        void GivenNvmAlwaysInvalid()
        {
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](infra::Function<void(bool)> onDone)
                    {
                        onDone(false);
                    }));
        }

        void ConstructFixture()
        {
            GivenNvmAlwaysInvalid();

            services::ConfigData config{};
            config.defaultControlMode = 0;
            coordinator.emplace(
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

            bridge.emplace(motorServer, *coordinator);
            motorServer.SetAcknowledger(ackSpy);
            ExecuteAllActions();
        }

        void ResetCaptures()
        {
            lastSentMessageType = 0;
            selectModeResponseSent = false;
            lastSelectModeResponseActiveMode = services::FocMotorMode::torque;
            categoryErrorSent = false;
            lastCategoryErrorOriginCmd = 0;
            lastCategoryErrorReason = services::FocMotorCategoryError::busy;
            ackSpy.Reset();
        }

        void Dispatch(uint8_t msgType, hal::Can::Message data)
        {
            motorServer.HandleMessage(msgType, data);
            ExecuteAllActions();
        }
    };

    // ---- QueryMotorType: returns current active mode ----

    TEST_F(FocMotorCanBridgeTest, OnQueryMotorType_ReturnsTorqueByDefault)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(services::focQueryMotorTypeId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, services::focQueryMotorTypeId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
        EXPECT_EQ(lastSentMessageType, services::focMotorTypeResponseId);
    }

    // ---- SetPid*: accepted in matching mode, modeMismatch otherwise ----

    TEST_F(FocMotorCanBridgeTest, OnSetPidCurrent_InTorqueMode_AcksSuccess)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(7, 0);
        Dispatch(services::focSetPidCurrentId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, services::focSetPidCurrentId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidSpeed_InTorqueMode_SendsModeMismatch)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(7, 0);
        Dispatch(services::focSetPidSpeedId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, services::focSetPidSpeedId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::categoryError);
        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryErrorOriginCmd, services::focSetPidSpeedId);
        EXPECT_EQ(lastCategoryErrorReason, services::FocMotorCategoryError::modeMismatch);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidPosition_InTorqueMode_SendsModeMismatch)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(7, 0);
        Dispatch(services::focSetPidPositionId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, services::focSetPidPositionId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::categoryError);
        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryErrorOriginCmd, services::focSetPidPositionId);
        EXPECT_EQ(lastCategoryErrorReason, services::FocMotorCategoryError::modeMismatch);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyMechanical_AcksNotImplemented)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(services::focIdentifyMechanicalId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::notImplemented);
    }

    TEST_F(FocMotorCanBridgeTest, OnRequestTelemetry_AcksNotImplemented)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(services::focRequestTelemetryId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::notImplemented);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetEncoderResolution_AcksNotImplemented)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(3, 0);
        Dispatch(services::focSetEncoderResolutionId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::notImplemented);
    }

    TEST_F(FocMotorCanBridgeTest, OnConfigureTelemetryRate_AcksNotImplemented)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = 10;
        Dispatch(services::focConfigureTelemetryRateId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::notImplemented);
    }

    // ---- Start/Stop: delegate to active state machine, ACK success ----

    TEST_F(FocMotorCanBridgeTest, OnStart_DelegatesToCmdEnable_NoCrashInIdle)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(services::focStartId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            coordinator->ActiveStateMachine().CurrentState()));
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCanBridgeTest, OnStop_DelegatesToCmdDisable_NoCrashInIdle)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(services::focStopId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            coordinator->ActiveStateMachine().CurrentState()));
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    // ---- IdentifyElectrical: delegates to CmdCalibrate ----

    TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_TransitionsToCalibrating)
    {
        ConstructFixture();

        infra::Function<void(std::optional<std::size_t>)> capturedCb;
        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
            .WillOnce(Invoke([&capturedCb](const auto&,
                                 const infra::Function<void(std::optional<std::size_t>)>& cb)
                {
                    capturedCb = cb;
                }));

        Dispatch(services::focIdentifyElectricalId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(
            coordinator->ActiveStateMachine().CurrentState()));
    }

    // ---- ClearFault / EmergencyStop ----

    TEST_F(FocMotorCanBridgeTest, OnClearFault_NoCrashInIdle)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(services::focClearFaultId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            coordinator->ActiveStateMachine().CurrentState()));
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCanBridgeTest, OnEmergencyStop_DelegatesToCmdDisable_NoCrashInIdle)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(services::focEmergencyStopId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            coordinator->ActiveStateMachine().CurrentState()));
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    // ---- SelectControlMode: defers ACK, emits success ACK + response on OK ----

    TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_Speed_AcksSuccess_AndEmitsResponse)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                {
                    onDone(services::NvmStatus::Ok);
                }));

        ResetCaptures();
        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = static_cast<uint8_t>(services::FocMotorMode::speed);
        Dispatch(services::focSelectControlModeId, data);

        EXPECT_TRUE(selectModeResponseSent);
        EXPECT_EQ(lastSelectModeResponseActiveMode, services::FocMotorMode::speed);
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, services::focSelectControlModeId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_NvmFailed_AcksPersistenceFailed)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                {
                    onDone(services::NvmStatus::WriteFailed);
                }));

        ResetCaptures();
        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = static_cast<uint8_t>(services::FocMotorMode::speed);
        Dispatch(services::focSelectControlModeId, data);

        EXPECT_FALSE(selectModeResponseSent);
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, services::focSelectControlModeId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::categoryError);
        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryErrorOriginCmd, services::focSelectControlModeId);
        EXPECT_EQ(lastCategoryErrorReason, services::FocMotorCategoryError::persistenceFailed);
    }

    // ---- Setpoint mismatch produces modeMismatch ACK ----

    TEST_F(FocMotorCanBridgeTest, OnSetTorqueSetpoint_AcceptedInTorqueModeDefault)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(3, 0);
        Dispatch(services::focSetTorqueSetpointId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetTorqueSetpoint_RejectedInSpeedMode)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                {
                    onDone(services::NvmStatus::Ok);
                }));

        hal::Can::Message selectData;
        selectData.resize(2, 0);
        selectData[1] = static_cast<uint8_t>(services::FocMotorMode::speed);
        Dispatch(services::focSelectControlModeId, selectData);

        ResetCaptures();
        hal::Can::Message data;
        data.resize(3, 0);
        Dispatch(services::focSetTorqueSetpointId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, services::focSetTorqueSetpointId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::categoryError);
        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryErrorReason, services::FocMotorCategoryError::modeMismatch);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetSpeedSetpoint_AcceptedInSpeedMode)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                {
                    onDone(services::NvmStatus::Ok);
                }));

        hal::Can::Message selectData;
        selectData.resize(2, 0);
        selectData[1] = static_cast<uint8_t>(services::FocMotorMode::speed);
        Dispatch(services::focSelectControlModeId, selectData);

        ResetCaptures();
        hal::Can::Message data;
        data.resize(3, 0);
        Dispatch(services::focSetSpeedSetpointId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetSpeedSetpoint_RejectedInDefaultTorqueMode)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(3, 0);
        Dispatch(services::focSetSpeedSetpointId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, services::focSetSpeedSetpointId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::categoryError);
        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryErrorReason, services::FocMotorCategoryError::modeMismatch);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPositionSetpoint_AcceptedInPositionMode)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                {
                    onDone(services::NvmStatus::Ok);
                }));

        hal::Can::Message selectData;
        selectData.resize(2, 0);
        selectData[1] = static_cast<uint8_t>(services::FocMotorMode::position);
        Dispatch(services::focSelectControlModeId, selectData);

        ResetCaptures();
        hal::Can::Message data;
        data.resize(3, 0);
        Dispatch(services::focSetPositionSetpointId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPositionSetpoint_RejectedInDefaultTorqueMode)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(3, 0);
        Dispatch(services::focSetPositionSetpointId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->commandType, services::focSetPositionSetpointId);
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::categoryError);
        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryErrorReason, services::FocMotorCategoryError::modeMismatch);
    }
}
