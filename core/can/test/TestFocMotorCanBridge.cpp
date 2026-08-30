#pragma GCC optimize("O3", "fast-math")

#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
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

    struct AcknowledgerSpy : services::CanCommandAcknowledger
    {
        struct Entry
        {
            uint8_t category{};
            uint8_t commandType{};
            services::CanAckStatus status{};
        };

        std::optional<Entry> last;
        std::size_t count{};

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
        : public Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        StrictMock<infra::StreamWriterMock> streamWriterMock;
        StrictMock<hal::SerialCommunicationMock> serialCommunicationMock;

        infra::Execute setupStreamExpectations{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(_, _)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Available()).Times(AnyNumber()).WillRepeatedly(Return(1000));
                EXPECT_CALL(streamWriterMock, ConstructSaveMarker()).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, GetProcessedBytesSince(_)).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, SaveState(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(streamWriterMock, RestoreState(_)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Overwrite(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(serialCommunicationMock, SendDataMock(_)).Times(AnyNumber());
            } };

        infra::TextOutputStream::WithErrorPolicy tracerStream{ streamWriterMock };
        services::TracerToStream tracer{ tracerStream };
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ serialCommunicationMock, tracer };
        services::TerminalWithStorage::WithMaxSize<20> terminal{ terminalWithCommands, tracer };

        FocMotorCanBridgeTest()
        {
            EXPECT_CALL(inverterMock, MaxCurrentSupported())
                .Times(AnyNumber())
                .WillRepeatedly(Return(foc::Ampere{ 15.0f }));
            EXPECT_CALL(inverterMock, BaseFrequency())
                .Times(AnyNumber())
                .WillRepeatedly(Return(hal::Hertz{ 10000 }));
            EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
            EXPECT_CALL(inverterMock, Start()).Times(AnyNumber());
            EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
            EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
            EXPECT_CALL(lowPriorityInterruptMock, Unregister()).Times(AnyNumber());
            EXPECT_CALL(faultNotifierMock, Register(_))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
                    {
                        faultNotifierMock.StoreHandler(handler);
                    }));

            EXPECT_CALL(canMock, SendData(_, _, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                    {
                        lastSentMsgType = services::ExtractCanMessageType(id.Get29BitId());
                        lastSentData = msg;
                        if (lastSentMsgType == services::canCategoryErrorResponseMessageTypeId && msg.size() >= 2)
                        {
                            categoryErrorSent = true;
                            lastCategoryErrorOriginCmd = msg[0];
                            lastCategoryError = static_cast<can::FocMotorCategoryError>(msg[1]);
                        }
                        if (lastSentMsgType == can::focSelectControlModeResponseId && !msg.empty())
                            selectResponseSent = true;
                        cb(true);
                    }));
        }

        void ConstructFixture()
        {
            GivenNvmAlwaysInvalid();
            Construct();
        }

        void ConstructFixtureInReady()
        {
            GivenNvmHoldsValidCalibration();
            Construct();
        }

        void GivenNvmAlwaysInvalid()
        {
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](infra::Function<void(bool)> done) { done(false); }));
        }

        void GivenNvmHoldsValidCalibration()
        {
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](infra::Function<void(bool)> done) { done(true); }));
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](services::CalibrationData& data, infra::Function<void(services::NvmStatus)> done)
                    {
                        data = services::CalibrationData{};
                        data.polePairs = 4;
                        data.rPhase = 0.5f;
                        data.lD = 1.0f;
                        data.lQ = 1.0f;
                        done(services::NvmStatus::Ok);
                    }));
        }

        void Construct()
        {
            services::ConfigData config{};
            config.defaultControlMode = 0;
            coordinator.emplace(
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
                faultNotifierMock,
                config,
                state_machine::ControlModeStateMachine::OuterLoopArgs{
                    foc::Ampere{ 10.0f },
                    hal::Hertz{ 1000 },
                    lowPriorityInterruptMock });

            bridge.emplace(*motorServer, *coordinator, inverterMock, electricalIdentMock, &mechIdentMock, foc::NewtonMeter{ 0.1f }, nvmMock, config, tracer);
            motorServer->SetAcknowledger(ackSpy);
            ExecuteAllActions();
        }

        void GivenModeSelected(can::FocMotorMode mode)
        {
            EXPECT_CALL(nvmMock, SaveConfig(_, _))
                .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
                    {
                        done(services::NvmStatus::Ok);
                    }));

            hal::Can::Message data;
            data.resize(2, 0);
            data[1] = static_cast<uint8_t>(mode);
            Dispatch(can::focSelectControlModeId, data);
        }

        void ResetCaptures()
        {
            lastSentMsgType = 0;
            categoryErrorSent = false;
            lastCategoryErrorOriginCmd = 0;
            lastCategoryError = can::FocMotorCategoryError::busy;
            selectResponseSent = false;
            ackSpy.Reset();
        }

        void Dispatch(uint8_t msgType, hal::Can::Message data)
        {
            motorServer->HandleMessage(msgType, data);
            ExecuteAllActions();
        }

        void DispatchSetpoint(uint8_t commandId, int16_t wireValue)
        {
            hal::Can::Message data;
            data.resize(3, 0);
            services::CanFrameCodec::WriteInt16(data, 1, wireValue);
            Dispatch(commandId, data);
        }

        void DispatchUInt32Command(uint8_t commandId, uint32_t value)
        {
            hal::Can::Message data;
            data.resize(5, 0);
            services::CanFrameCodec::WriteUInt32(data, 1, value);
            Dispatch(commandId, data);
        }

        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<services::MechanicalParametersIdentificationMock> mechIdentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;

        StrictMock<hal::CanMock> canMock;
        services::CanFrameTransport canTransport{ canMock, 1 };
        std::optional<can::FocMotorCategoryServer> motorServer{ std::in_place, canTransport };
        AcknowledgerSpy ackSpy;

        std::optional<state_machine::ControlModeStateMachine> coordinator;
        std::optional<can::FocMotorCanBridge> bridge;

        uint8_t lastSentMsgType{};
        hal::Can::Message lastSentData;
        bool categoryErrorSent{ false };
        uint8_t lastCategoryErrorOriginCmd{};
        can::FocMotorCategoryError lastCategoryError{ can::FocMotorCategoryError::busy };
        bool selectResponseSent{ false };
    };

    // REQ-INT-001
    TEST_F(FocMotorCanBridgeTest, OnStart_InReady_AcksSuccess)
    {
        ConstructFixtureInReady();
        ResetCaptures();

        Dispatch(can::focStartId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    // REQ-INT-002
    TEST_F(FocMotorCanBridgeTest, OnStop_InEnabled_AcksSuccess)
    {
        ConstructFixtureInReady();
        Dispatch(can::focStartId, {});
        ResetCaptures();

        Dispatch(can::focStopId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    // REQ-INT-003
    TEST_F(FocMotorCanBridgeTest, OnClearFault_InIdle_AcksInvalidState)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(can::focClearFaultId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    }

    // REQ-INT-005
    TEST_F(FocMotorCanBridgeTest, OnStart_InIdle_AcksInvalidState)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(can::focStartId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    }

    // REQ-INT-006
    TEST_F(FocMotorCanBridgeTest, OnEmergencyStop_InIdle_AcksSuccess)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(can::focEmergencyStopId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidCurrent_InTorqueMode_AcksSuccess)
    {
        ConstructFixture();
        ResetCaptures();

        DispatchSetpoint(can::focSetPidCurrentId, 1000);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidSpeed_InSpeedMode_AcksSuccess)
    {
        ConstructFixture();
        GivenModeSelected(can::FocMotorMode::speed);
        ResetCaptures();

        DispatchSetpoint(can::focSetPidSpeedId, 100);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidPosition_InPositionMode_AcksSuccess)
    {
        ConstructFixture();
        GivenModeSelected(can::FocMotorMode::position);
        ResetCaptures();

        DispatchSetpoint(can::focSetPidPositionId, 18);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
        EXPECT_FALSE(categoryErrorSent);
    }

    // REQ-CM-001 — SelectControlMode transitions the active mode
    TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_Speed_EmitsResponseAndAcksSuccess)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
                {
                    done(services::NvmStatus::Ok);
                }));

        ResetCaptures();
        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = static_cast<uint8_t>(can::FocMotorMode::speed);
        Dispatch(can::focSelectControlModeId, data);

        EXPECT_TRUE(selectResponseSent);
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    // REQ-CM-002 — SetTorqueSetpoint accepted in torque mode
    TEST_F(FocMotorCanBridgeTest, OnSetTorqueSetpoint_InReady_AcksSuccess)
    {
        ConstructFixtureInReady();
        ResetCaptures();

        DispatchSetpoint(can::focSetTorqueSetpointId, 100);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    // REQ-CM-003 — SetSpeedSetpoint accepted in speed mode
    TEST_F(FocMotorCanBridgeTest, OnSetSpeedSetpoint_InSpeedModeAndReady_AcksSuccess)
    {
        ConstructFixtureInReady();
        GivenModeSelected(can::FocMotorMode::speed);
        ResetCaptures();

        DispatchSetpoint(can::focSetSpeedSetpointId, 100);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    // REQ-CM-004 — SetPositionSetpoint accepted in position mode
    TEST_F(FocMotorCanBridgeTest, OnSetPositionSetpoint_InPositionModeAndReady_AcksSuccess)
    {
        ConstructFixtureInReady();
        GivenModeSelected(can::FocMotorMode::position);
        ResetCaptures();

        DispatchSetpoint(can::focSetPositionSetpointId, 100);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    // REQ-CM-005 — Setpoint rejected when mode does not match
    TEST_F(FocMotorCanBridgeTest, OnSetTorqueSetpoint_InSpeedMode_RejectsWithModeMismatch)
    {
        ConstructFixture();
        GivenModeSelected(can::FocMotorMode::speed);
        ResetCaptures();

        DispatchSetpoint(can::focSetTorqueSetpointId, 0);

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::modeMismatch);
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::categoryError);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetTorqueSetpoint_ExceedsInverterCurrent_RejectsWithInvalidPayload)
    {
        ConstructFixtureInReady();
        ResetCaptures();

        DispatchSetpoint(can::focSetTorqueSetpointId, 32767);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_UnrecognisedMode_RejectsWithInvalidPayload)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = 0xFF;
        Dispatch(can::focSelectControlModeId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
        EXPECT_FALSE(selectResponseSent);
    }

    // OnSelectControlMode — busy path (second select while one is pending)
    TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_WhilePending_RejectsBusy)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _)).Times(AnyNumber());

        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = static_cast<uint8_t>(can::FocMotorMode::speed);
        motorServer->HandleMessage(can::focSelectControlModeId, data);

        ResetCaptures();
        motorServer->HandleMessage(can::focSelectControlModeId, data);
        ExecuteAllActions();

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::busy);
    }

    // OnSelectControlMode — invalid mode byte
    TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_InvalidMode_RejectsInvalidPayload)
    {
        ConstructFixture();
        ResetCaptures();

        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = 0xFF;
        Dispatch(can::focSelectControlModeId, data);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
        EXPECT_FALSE(selectResponseSent);
    }

    // OnSelectControlMode — select rejected (motor enabled)
    TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_WhenEnabled_RejectsWithCategoryError)
    {
        ConstructFixtureInReady();
        Dispatch(can::focStartId, {});
        ResetCaptures();

        EXPECT_CALL(nvmMock, SaveConfig(_, _)).Times(0);

        hal::Can::Message data;
        data.resize(2, 0);
        data[1] = static_cast<uint8_t>(can::FocMotorMode::speed);
        Dispatch(can::focSelectControlModeId, data);

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_FALSE(selectResponseSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_BroadcastsParamsAndAcksSuccess)
    {
        ConstructFixture();

        EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
            .WillOnce(Invoke([](const services::ElectricalParametersIdentification::ResistanceAndInductanceConfig&,
                               infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> done)
                {
                    done(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
                }));
        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
            .WillOnce(Invoke([](const services::ElectricalParametersIdentification::PolePairsConfig&,
                               infra::Function<void(std::optional<std::size_t>)> done)
                {
                    done(std::size_t{ 4 });
                }));

        ResetCaptures();
        Dispatch(can::focIdentifyElectricalId, {});

        EXPECT_EQ(lastSentMsgType, can::focElectricalParamsResponseId);
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_FailedResistance_SendsCalibrationFailed)
    {
        ConstructFixture();

        EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
            .WillOnce(Invoke([](const services::ElectricalParametersIdentification::ResistanceAndInductanceConfig&,
                               infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> done)
                {
                    done(services::ElectricalParametersIdentification::ResistanceInductanceResult{});
                }));

        ResetCaptures();
        Dispatch(can::focIdentifyElectricalId, {});

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::calibrationFailed);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyMechanical_InReady_BroadcastsParamsAndAcksSuccess)
    {
        ConstructFixtureInReady();

        EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
            .WillOnce(Invoke([](const foc::NewtonMeter&, std::size_t,
                               const services::MechanicalParametersIdentification::Config&,
                               infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>,
                                   std::optional<foc::NewtonMeterSecondSquared>)> done)
                {
                    done(foc::NewtonMeterSecondPerRadian{ 0.001f }, foc::NewtonMeterSecondSquared{ 0.002f });
                }));

        ResetCaptures();
        Dispatch(can::focIdentifyMechanicalId, {});

        EXPECT_EQ(lastSentMsgType, can::focMechanicalParamsResponseId);
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyMechanical_InIdle_RejectsInvalidState)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(can::focIdentifyMechanicalId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnRequestTelemetry_InIdle_BroadcastsIdleStatus)
    {
        ConstructFixture();
        ResetCaptures();

        Dispatch(can::focRequestTelemetryId, {});

        EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
        ASSERT_GE(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(can::FocMotorState::idle));
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::none));
        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    }

    TEST_F(FocMotorCanBridgeTest, OnRequestTelemetry_InEnabled_BroadcastsRunningStatus)
    {
        ConstructFixtureInReady();
        Dispatch(can::focStartId, {});
        ResetCaptures();

        Dispatch(can::focRequestTelemetryId, {});

        EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
        ASSERT_GE(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(can::FocMotorState::running));
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::none));
    }

    TEST_F(FocMotorCanBridgeTest, OnSetEncoderResolution_PersistsToNvmAndAcksSuccess)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
                {
                    done(services::NvmStatus::Ok);
                }));

        ResetCaptures();
        DispatchUInt32Command(can::focSetEncoderResolutionId, 8000);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetEncoderResolution_ZeroResolution_RejectsInvalidPayload)
    {
        ConstructFixture();
        ResetCaptures();

        DispatchUInt32Command(can::focSetEncoderResolutionId, 0);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnConfigureTelemetryRate_PersistsToNvmAndAcksSuccess)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
                {
                    done(services::NvmStatus::Ok);
                }));

        ResetCaptures();
        DispatchUInt32Command(can::focConfigureTelemetryRateId, 200);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnConfigureTelemetryRate_ZeroRate_RejectsInvalidPayload)
    {
        ConstructFixture();
        ResetCaptures();

        DispatchUInt32Command(can::focConfigureTelemetryRateId, 0);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
        EXPECT_FALSE(categoryErrorSent);
    }

    // REQ-INT-013 — BroadcastFault sends telemetry status frame with fault state
    TEST_F(FocMotorCanBridgeTest, BroadcastFault_Overcurrent_EmitsTelemetryStatusFrameWithFaultState)
    {
        ConstructFixture();
        ResetCaptures();

        bridge->BroadcastFault(state_machine::FaultCode::overcurrent);

        EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
        ASSERT_GE(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(can::FocMotorState::fault));
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::overCurrent));
    }

    TEST_F(FocMotorCanBridgeTest, BroadcastFault_Overvoltage_MapsToOverVoltage)
    {
        ConstructFixture();
        ResetCaptures();

        bridge->BroadcastFault(state_machine::FaultCode::overvoltage);

        EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
        ASSERT_GE(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::overVoltage));
    }

    TEST_F(FocMotorCanBridgeTest, BroadcastFault_Overtemperature_MapsToOverTemperature)
    {
        ConstructFixture();
        ResetCaptures();

        bridge->BroadcastFault(state_machine::FaultCode::overtemperature);

        EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
        ASSERT_GE(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::overTemperature));
    }

    TEST_F(FocMotorCanBridgeTest, BroadcastFault_EncoderLoss_MapsToSensorFault)
    {
        ConstructFixture();
        ResetCaptures();

        bridge->BroadcastFault(state_machine::FaultCode::encoderLoss);

        EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
        ASSERT_GE(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::sensorFault));
    }

    TEST_F(FocMotorCanBridgeTest, BroadcastFault_HardwareFault_MapsToNone)
    {
        ConstructFixture();
        ResetCaptures();

        bridge->BroadcastFault(state_machine::FaultCode::hardwareFault);

        EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
        ASSERT_GE(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::none));
    }

    TEST_F(FocMotorCanBridgeTest, BroadcastFault_TelemetryFrame_HasSixBytesWithZeroSpeedAndPosition)
    {
        ConstructFixture();
        ResetCaptures();

        bridge->BroadcastFault(state_machine::FaultCode::overcurrent);

        ASSERT_EQ(lastSentData.size(), 6u);
        EXPECT_EQ(services::CanFrameCodec::ReadInt16(lastSentData, 2), 0);
        EXPECT_EQ(services::CanFrameCodec::ReadInt16(lastSentData, 4), 0);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidSpeed_InTorqueMode_RejectsInvalidPayload)
    {
        ConstructFixture();
        ResetCaptures();

        DispatchSetpoint(can::focSetPidSpeedId, 100);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetPidPosition_InTorqueMode_RejectsInvalidPayload)
    {
        ConstructFixture();
        ResetCaptures();

        DispatchSetpoint(can::focSetPidPositionId, 18);

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
        EXPECT_FALSE(categoryErrorSent);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyMechanical_NullMechIdent_ReturnsNotImplemented)
    {
        GivenNvmAlwaysInvalid();
        services::ConfigData config{};
        coordinator.emplace(
            application::TerminalAndTracer{ terminal, tracer },
            application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
            nvmMock,
            application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
            faultNotifierMock,
            config,
            state_machine::ControlModeStateMachine::OuterLoopArgs{
                foc::Ampere{ 10.0f },
                hal::Hertz{ 1000 },
                lowPriorityInterruptMock });
        bridge.emplace(*motorServer, *coordinator, inverterMock, electricalIdentMock, nullptr, foc::NewtonMeter{ 0.1f }, nvmMock, config, tracer);
        motorServer->SetAcknowledger(ackSpy);
        ExecuteAllActions();
        ResetCaptures();

        Dispatch(can::focIdentifyMechanicalId, {});

        ASSERT_TRUE(ackSpy.last.has_value());
        EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::notImplemented);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_SecondStepFails_SendsCalibrationFailed)
    {
        ConstructFixture();

        EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
            .WillOnce(Invoke([](const services::ElectricalParametersIdentification::ResistanceAndInductanceConfig&,
                               infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> done)
                {
                    done(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
                }));
        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
            .WillOnce(Invoke([](const services::ElectricalParametersIdentification::PolePairsConfig&,
                               infra::Function<void(std::optional<std::size_t>)> done)
                {
                    done(std::nullopt);
                }));

        ResetCaptures();
        Dispatch(can::focIdentifyElectricalId, {});

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::calibrationFailed);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_WhilePending_ReturnsBusy)
    {
        ConstructFixture();

        infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> capturedCallback;
        EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
            .WillOnce(Invoke([&capturedCallback](const services::ElectricalParametersIdentification::ResistanceAndInductanceConfig&,
                               infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> done)
                {
                    capturedCallback = done;
                }));

        motorServer->HandleMessage(can::focIdentifyElectricalId, {});

        ResetCaptures();
        Dispatch(can::focIdentifyElectricalId, {});

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::busy);
    }

    TEST_F(FocMotorCanBridgeTest, OnIdentifyMechanical_WhilePending_ReturnsBusy)
    {
        ConstructFixtureInReady();

        infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)> capturedCallback;
        EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
            .WillOnce(Invoke([&capturedCallback](const foc::NewtonMeter&, std::size_t,
                               const services::MechanicalParametersIdentification::Config&,
                               infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>,
                                   std::optional<foc::NewtonMeterSecondSquared>)> done)
                {
                    capturedCallback = done;
                }));

        motorServer->HandleMessage(can::focIdentifyMechanicalId, {});

        ResetCaptures();
        Dispatch(can::focIdentifyMechanicalId, {});

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::busy);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetEncoderResolution_NvmFails_SendsPersistenceFailed)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
                {
                    done(services::NvmStatus::WriteFailed);
                }));

        ResetCaptures();
        DispatchUInt32Command(can::focSetEncoderResolutionId, 4000);

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::persistenceFailed);
    }

    TEST_F(FocMotorCanBridgeTest, OnSetEncoderResolution_WhileNvmPending_ReturnsBusy)
    {
        ConstructFixture();

        infra::Function<void(services::NvmStatus)> capturedNvmCallback;
        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([&capturedNvmCallback](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
                {
                    capturedNvmCallback = done;
                }));

        motorServer->HandleMessage(can::focSetEncoderResolutionId, []{
            hal::Can::Message d; d.resize(5, 0);
            services::CanFrameCodec::WriteUInt32(d, 1, 4000);
            return d;
        }());

        ResetCaptures();
        DispatchUInt32Command(can::focSetEncoderResolutionId, 8000);

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::busy);
    }

    TEST_F(FocMotorCanBridgeTest, OnConfigureTelemetryRate_NvmFails_SendsPersistenceFailed)
    {
        ConstructFixture();

        EXPECT_CALL(nvmMock, SaveConfig(_, _))
            .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
                {
                    done(services::NvmStatus::WriteFailed);
                }));

        ResetCaptures();
        DispatchUInt32Command(can::focConfigureTelemetryRateId, 100);

        EXPECT_TRUE(categoryErrorSent);
        EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::persistenceFailed);
    }

    TEST_F(FocMotorCanBridgeTest, OnRequestTelemetry_InFaultState_BroadcastsFaultStatus)
    {
        ConstructFixtureInReady();
        Dispatch(can::focStartId, {});
        faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
        ExecuteAllActions();
        ResetCaptures();

        Dispatch(can::focRequestTelemetryId, {});

        EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
        ASSERT_GE(lastSentData.size(), 2u);
        EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(can::FocMotorState::fault));
        EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::overCurrent));
    }

    // REQ-INT-012 — ctor emits one trace line
    TEST_F(FocMotorCanBridgeTest, Constructor_EmitsTraceMessage)
    {
        EXPECT_CALL(streamWriterMock, Insert(_, _)).Times(AtLeast(1));
        EXPECT_CALL(nvmMock, IsCalibrationValid(_))
            .WillRepeatedly(Invoke([](infra::Function<void(bool)> done) { done(false); }));

        services::ConfigData config{};
        coordinator.emplace(
            application::TerminalAndTracer{ terminal, tracer },
            application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
            nvmMock,
            application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
            faultNotifierMock,
            config,
            state_machine::ControlModeStateMachine::OuterLoopArgs{
                foc::Ampere{ 10.0f },
                hal::Hertz{ 1000 },
                lowPriorityInterruptMock });

        bridge.emplace(*motorServer, *coordinator, inverterMock, electricalIdentMock, &mechIdentMock, foc::NewtonMeter{ 0.1f }, nvmMock, config, tracer);
        motorServer->SetAcknowledger(ackSpy);
        ExecuteAllActions();
    }
}
