#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
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

namespace
{
    using namespace testing;
    using namespace services;

    using TestedControlMode = state_machine::ControlModeStateMachine<
        foc::FocTorqueImpl,
        foc::FocSpeedImpl,
        foc::FocPositionImpl>;

    using BridgeType = state_machine::FocMotorCanBridge<
        foc::FocTorqueImpl,
        foc::FocSpeedImpl,
        foc::FocPositionImpl>;

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
        bool commandRejectedSent{ false };
        bool selectResponseSent{ false };
        services::FocRejectReason lastSelectResponseReason{ services::FocRejectReason::ok };
        services::FocRejectReason lastCommandRejectedReason{ services::FocRejectReason::ok };
        uint8_t lastCommandRejectedOrigCmdId{ 0 };

        StrictMock<hal::CanMock> canMock;

        infra::Execute setupCanExpectations{ [this]()
            {
                EXPECT_CALL(canMock, SendData(_, _, _))
                    .Times(AnyNumber())
                    .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                        {
                            lastSentMessageType = services::ExtractCanMessageType(id.Get29BitId());
                            if (lastSentMessageType == services::focCommandRejectedResponseId)
                            {
                                commandRejectedSent = true;
                                lastCommandRejectedOrigCmdId = msg[0];
                                lastCommandRejectedReason = static_cast<services::FocRejectReason>(msg[1]);
                            }
                            if (lastSentMessageType == services::focSelectControlModeResponseId)
                            {
                                selectResponseSent = true;
                                lastSelectResponseReason = static_cast<services::FocRejectReason>(msg[1]);
                            }
                            cb(true);
                        }));
            } };

        services::CanFrameTransport canTransport{ canMock, 1 };
        services::FocMotorCategoryServer motorServer{ canTransport };

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
                application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
                faultNotifierMock,
                config,
                TestedControlMode::OuterLoopArgs{
                    foc::Ampere{ 10.0f },
                    hal::Hertz{ 1000 },
                    lowPriorityInterruptMock });

            bridge.emplace(motorServer, *coordinator);
            ExecuteAllActions();
        }

        void ResetSentFlags()
        {
            lastSentMessageType = 0;
            commandRejectedSent = false;
            selectResponseSent = false;
            lastSelectResponseReason = services::FocRejectReason::ok;
            lastCommandRejectedReason = services::FocRejectReason::ok;
            lastCommandRejectedOrigCmdId = 0;
        }

        void InjectMessage(uint8_t msgType, hal::Can::Message data)
        {
            motorServer.HandleMessage(msgType, data);
            ExecuteAllActions();
        }
    };

    // ---- No-op handlers: verify no crash and no rejection ----

    TEST_F(FocMotorCanBridgeTest, OnQueryMotorType_IsNoOp)
    {
        ConstructFixture();
        ResetSentFlags();

        InjectMessage(services::focQueryMotorTypeId, {});

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidCurrent_IsNoOp)
    {
        ConstructFixture();
        ResetSentFlags();

        hal::Can::Message data;
        data.resize(6, 0);
        InjectMessage(services::focSetPidCurrentId, data);

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidSpeed_IsNoOp)
    {
        ConstructFixture();
        ResetSentFlags();

        hal::Can::Message data;
        data.resize(6, 0);
        InjectMessage(services::focSetPidSpeedId, data);

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidPosition_IsNoOp)
    {
        ConstructFixture();
        ResetSentFlags();

        hal::Can::Message data;
        data.resize(6, 0);
        InjectMessage(services::focSetPidPositionId, data);

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyMechanical_IsNoOp)
    {
        ConstructFixture();
        ResetSentFlags();

        InjectMessage(services::focIdentifyMechanicalId, {});

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnRequestTelemetry_IsNoOp)
    {
        ConstructFixture();
        ResetSentFlags();

        InjectMessage(services::focRequestTelemetryId, {});

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetEncoderResolution_IsNoOp)
    {
        ConstructFixture();
        ResetSentFlags();

        hal::Can::Message data;
        data.resize(2, 0);
        InjectMessage(services::focSetEncoderResolutionId, data);

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnConfigureTelemetryRate_IsNoOp)
    {
        ConstructFixture();
        ResetSentFlags();

        hal::Can::Message data;
        data.push_back(10);
        InjectMessage(services::focConfigureTelemetryRateId, data);

        EXPECT_FALSE(commandRejectedSent);
    }

    // ---- Start/Stop: delegate to active state machine ----

    TEST_F(FocMotorCanBridgeTest, OnStart_Calls_CmdEnable_Is_NoOp_In_Idle)
    {
        ConstructFixture();

        // CmdEnable() from Idle is a no-op — no crash expected
        InjectMessage(services::focStartId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            coordinator->ActiveStateMachine().CurrentState()));
    }

    TEST_F(FocMotorCanBridgeTest, OnStop_Calls_CmdDisable_Is_NoOp_In_Idle)
    {
        ConstructFixture();

        InjectMessage(services::focStopId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            coordinator->ActiveStateMachine().CurrentState()));
    }

    // ---- IdentifyElectrical: delegates to CmdCalibrate ----

    TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_Calls_CmdCalibrate)
    {
        ConstructFixture();

        infra::Function<void(std::optional<std::size_t>)> capturedCb;
        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
            .WillOnce(Invoke([&capturedCb](const auto&,
                                 const infra::Function<void(std::optional<std::size_t>)>& cb)
                {
                    capturedCb = cb;
                }));

        InjectMessage(services::focIdentifyElectricalId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(
            coordinator->ActiveStateMachine().CurrentState()));
    }

    // ---- ClearFault: delegates to CmdClearFault ----

    TEST_F(FocMotorCanBridgeTest, OnClearFault_Calls_CmdClearFault_Is_NoOp_In_Idle)
    {
        ConstructFixture();

        InjectMessage(services::focClearFaultId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            coordinator->ActiveStateMachine().CurrentState()));
    }

    // ---- EmergencyStop: delegates to CmdDisable ----

    TEST_F(FocMotorCanBridgeTest, OnEmergencyStop_Calls_CmdDisable_Is_NoOp_In_Idle)
    {
        ConstructFixture();

        InjectMessage(services::focEmergencyStopId, {});

        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
            coordinator->ActiveStateMachine().CurrentState()));
    }

    // ---- SelectControlMode: sends response ----

    TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_Speed_Sends_Response)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                {
                    onDone(services::NvmStatus::Ok);
                }));

        ResetSentFlags();
        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = static_cast<uint8_t>(services::FocMotorMode::speed);
        InjectMessage(services::focSelectControlModeId, data);

        EXPECT_TRUE(selectResponseSent);
        EXPECT_EQ(lastSelectResponseReason, services::FocRejectReason::ok);
    }

    TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_NvmFailed_Sends_Response_With_NvmFailed_Reason)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                {
                    onDone(services::NvmStatus::WriteFailed);
                }));

        ResetSentFlags();
        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = static_cast<uint8_t>(services::FocMotorMode::speed);
        InjectMessage(services::focSelectControlModeId, data);

        EXPECT_TRUE(selectResponseSent);
        EXPECT_EQ(lastSelectResponseReason, services::FocRejectReason::nvmFailed);
    }

    // ---- SetTorqueSetpoint ----

    TEST_F(FocMotorCanBridgeTest, OnSetTorqueSetpoint_Accepted_In_Default_Torque_Mode)
    {
        ConstructFixture();
        ResetSentFlags();

        hal::Can::Message data;
        data.resize(3, 0);
        InjectMessage(services::focSetTorqueSetpointId, data);

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetTorqueSetpoint_Rejected_In_Speed_Mode)
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
        InjectMessage(services::focSelectControlModeId, selectData);

        ResetSentFlags();

        hal::Can::Message data;
        data.resize(3, 0);
        InjectMessage(services::focSetTorqueSetpointId, data);

        EXPECT_TRUE(commandRejectedSent);
        EXPECT_EQ(lastCommandRejectedOrigCmdId, services::focSetTorqueSetpointId);
        EXPECT_EQ(lastCommandRejectedReason, services::FocRejectReason::controlModeMismatch);
    }

    // ---- SetSpeedSetpoint ----

    TEST_F(FocMotorCanBridgeTest, OnSetSpeedSetpoint_Accepted_In_Speed_Mode)
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
        InjectMessage(services::focSelectControlModeId, selectData);

        ResetSentFlags();

        hal::Can::Message data;
        data.resize(3, 0);
        InjectMessage(services::focSetSpeedSetpointId, data);

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetSpeedSetpoint_Rejected_In_Default_Torque_Mode)
    {
        ConstructFixture();
        ResetSentFlags();

        hal::Can::Message data;
        data.resize(3, 0);
        InjectMessage(services::focSetSpeedSetpointId, data);

        EXPECT_TRUE(commandRejectedSent);
        EXPECT_EQ(lastCommandRejectedOrigCmdId, services::focSetSpeedSetpointId);
        EXPECT_EQ(lastCommandRejectedReason, services::FocRejectReason::controlModeMismatch);
    }

    // ---- SetPositionSetpoint ----

    TEST_F(FocMotorCanBridgeTest, OnSetPositionSetpoint_Accepted_In_Position_Mode)
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
        InjectMessage(services::focSelectControlModeId, selectData);

        ResetSentFlags();

        hal::Can::Message data;
        data.resize(3, 0);
        InjectMessage(services::focSetPositionSetpointId, data);

        EXPECT_FALSE(commandRejectedSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPositionSetpoint_Rejected_In_Default_Torque_Mode)
    {
        ConstructFixture();
        ResetSentFlags();

        hal::Can::Message data;
        data.resize(3, 0);
        InjectMessage(services::focSetPositionSetpointId, data);

        EXPECT_TRUE(commandRejectedSent);
        EXPECT_EQ(lastCommandRejectedOrigCmdId, services::focSetPositionSetpointId);
        EXPECT_EQ(lastCommandRejectedReason, services::FocRejectReason::controlModeMismatch);
    }
}
