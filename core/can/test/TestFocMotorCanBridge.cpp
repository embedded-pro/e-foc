#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/current_offsets/test_doubles/PhaseCurrentOffsetsMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/services/standstill/test_doubles/RotorStandstillMock.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <bit>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <optional>

namespace
{
    using namespace testing;

    struct AcknowledgerSpy
        : services::CanCommandAcknowledger
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
        , public infra::ClockFixture
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
        services::TerminalWithStorage::WithMaxSize<22> terminal{ terminalWithCommands, tracer };

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
            EXPECT_CALL(faultNotifierMock, Register(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& immediate, const infra::Function<void(state_machine::FaultCode)>& deferred)
                    {
                        faultNotifierMock.StoreHandler(immediate, deferred);
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

        void ConstructFixtureInPartialCalibration()
        {
            GivenNvmHoldsPartialCalibration();
            Construct();
        }

        void GivenNvmAlwaysInvalid()
        {
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](infra::Function<void(bool)> done)
                    {
                        done(false);
                    }));
        }

        void GivenNvmHoldsValidCalibration()
        {
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](infra::Function<void(bool)> done)
                    {
                        done(true);
                    }));
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](services::CalibrationData& data, infra::Function<void(services::NvmStatus)> done)
                    {
                        data = services::CalibrationData{};
                        data.polePairs = 4;
                        data.rPhase = 0.5f;
                        data.lD = 1.0f;
                        data.lQ = 1.0f;
                        data.inertia = 0.005f;
                        data.stage = services::CalibrationStage::complete;
                        done(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](const services::CalibrationData&,
                                           infra::Function<void(services::NvmStatus)> done)
                    {
                        done(services::NvmStatus::Ok);
                    }));
        }

        void GivenNvmHoldsPartialCalibration()
        {
            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](infra::Function<void(bool)> done)
                    {
                        done(true);
                    }));
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](services::CalibrationData& data, infra::Function<void(services::NvmStatus)> done)
                    {
                        data = services::CalibrationData{};
                        data.polePairs = 4;
                        data.rPhase = 0.5f;
                        data.stage = services::CalibrationStage::none;
                        done(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](const services::CalibrationData&,
                                           infra::Function<void(services::NvmStatus)> done)
                    {
                        done(services::NvmStatus::Ok);
                    }));
        }

        void AlignAfterBoot()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                .WillOnce(Invoke([](std::size_t, const auto&,
                                     const infra::Function<void(std::optional<foc::Radians>)>& cb)
                    {
                        cb(foc::Radians{ 0.0f });
                    }));
            DispatchCommand(can::focAlignId);
        }

        void ConstructFixtureInReady()
        {
            GivenNvmHoldsValidCalibration();
            Construct();
            AlignAfterBoot();
        }

        void Construct()
        {
            services::ConfigData config{};
            config.defaultControlMode = 0;
            coordinator.emplace(
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), foc::Weber{ 0.007f } },
                faultNotifierMock,
                config,
                state_machine::ControlModeStateMachine::OuterLoopArgs{
                    foc::Ampere{ 10.0f },
                    hal::Hertz{ 1000 },
                    lowPriorityInterruptMock });

            std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> offsets;
            std::optional<std::reference_wrapper<services::RotorStandstill>> rest;
            if (bridgeMeasuresOffsets)
            {
                offsets = currentOffsetsMock;
                rest = standstillMock;
            }

            bridge.emplace(*motorServer, *coordinator, can::FocMotorCanBridge::Platform{ inverterMock, encoderMock, diagnostics, can::FocResetCause::watchdog }, electricalIdentMock, tracer, offsets, rest);
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

        void ExpectConfigSaved(services::NvmStatus status = services::NvmStatus::Ok)
        {
            EXPECT_CALL(nvmMock, SaveConfig(_, _))
                .WillOnce(Invoke([this, status](const services::ConfigData& data, infra::Function<void(services::NvmStatus)> done)
                    {
                        savedConfig = data;
                        done(status);
                    }));
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

        services::CanDispatchResult Dispatch(uint8_t msgType, hal::Can::Message data)
        {
            const auto result = motorServer->HandleMessage(msgType, data);
            ExecuteAllActions();
            return result;
        }

        static hal::Can::Message SequenceOnlyPayload()
        {
            hal::Can::Message data;
            data.resize(1, 0);
            return data;
        }

        void DispatchCommand(uint8_t commandId)
        {
            Dispatch(commandId, SequenceOnlyPayload());
        }

        void DispatchSetpoint(uint8_t commandId, int16_t wireValue)
        {
            hal::Can::Message data;
            data.resize(3, 0);
            services::CanFrameCodec::WriteInt16(data, 1, wireValue);
            Dispatch(commandId, data);
        }

        static hal::Can::Message Bytes(std::initializer_list<uint8_t> values)
        {
            hal::Can::Message message;
            for (const auto value : values)
                message.push_back(value);
            return message;
        }

        void DispatchSelectAlgorithm(can::FocControlLoop loop, uint8_t algorithm)
        {
            hal::Can::Message data;
            data.resize(3, 0);
            data[1] = static_cast<uint8_t>(loop);
            data[2] = algorithm;
            Dispatch(can::focSelectAlgorithmId, data);
        }

        void DispatchSetParameter(can::FocParameter parameter, float value)
        {
            hal::Can::Message data;
            data.resize(6, 0);
            data[1] = static_cast<uint8_t>(parameter);
            services::CanFrameCodec::WriteUInt32(data, 2, std::bit_cast<uint32_t>(value));
            Dispatch(can::focSetParameterId, data);
        }

        void DispatchQueryParameter(can::FocParameter parameter)
        {
            hal::Can::Message data;
            data.resize(2, 0);
            data[1] = static_cast<uint8_t>(parameter);
            Dispatch(can::focQueryParameterId, data);
        }

        can::FocParameterValue AnsweredParameter() const
        {
            EXPECT_EQ(lastSentMsgType, can::focParameterResponseId);
            EXPECT_EQ(lastSentData.size(), 6u);
            if (lastSentData.size() != 6)
                return {};

            return can::FocParameterValue{ std::bit_cast<float>(services::CanFrameCodec::ReadUInt32(lastSentData, 2)), (lastSentData[1] & can::focParameterAutomaticFlag) != 0 };
        }

        void ExpectAnswered(float value, bool automatic) const
        {
            const auto answer = AnsweredParameter();
            EXPECT_NEAR(answer.value, value, std::abs(value) * 1e-6f + 1e-9f);
            EXPECT_EQ(answer.automatic, automatic);
        }

        void ExpectCalibrationSaved(services::NvmStatus status = services::NvmStatus::Ok)
        {
            EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                .WillOnce(Invoke([this, status](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> done)
                    {
                        savedCalibration = data;
                        done(status);
                    }));
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
        StrictMock<services::PhaseCurrentOffsetsMock> currentOffsetsMock;
        StrictMock<services::RotorStandstillMock> standstillMock;
        bool bridgeMeasuresOffsets{ false };
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;
        infra::Execute setupTeardownExpectations{ [this]()
            {
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(electricalIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
                EXPECT_CALL(encoderMock, Read()).WillRepeatedly(Return(foc::Radians{ 0.0f }));
            } };

        StrictMock<hal::CanMock> canMock;
        services::CanFrameTransport canTransport{ canMock, 1 };
        std::optional<can::FocMotorCategoryServer> motorServer{ std::in_place, canTransport };
        AcknowledgerSpy ackSpy;

        std::optional<state_machine::ControlModeStateMachine> coordinator;
        application::ControlLoopMetrics loopMetrics;
        application::PlatformDiagnostics diagnostics{ loopMetrics };
        std::optional<can::FocMotorCanBridge> bridge;

        uint8_t lastSentMsgType{};
        hal::Can::Message lastSentData;
        bool categoryErrorSent{ false };
        uint8_t lastCategoryErrorOriginCmd{};
        can::FocMotorCategoryError lastCategoryError{ can::FocMotorCategoryError::busy };
        bool selectResponseSent{ false };
        std::optional<services::ConfigData> savedConfig;
        std::optional<services::CalibrationData> savedCalibration;
    };
}

TEST_F(FocMotorCanBridgeTest, OnStart_InReady_AcksSuccess)
{
    ConstructFixtureInReady();
    ResetCaptures();

    DispatchCommand(can::focStartId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnStop_InEnabled_AcksSuccess)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    DispatchCommand(can::focStopId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnClearFault_InIdle_AcksInvalidState)
{
    ConstructFixture();
    ResetCaptures();

    DispatchCommand(can::focClearFaultId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCanBridgeTest, OnStart_InIdle_AcksInvalidState)
{
    ConstructFixture();
    ResetCaptures();

    DispatchCommand(can::focStartId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCanBridgeTest, OnEmergencyStop_InIdle_AcksSuccess)
{
    ConstructFixture();
    ResetCaptures();

    DispatchCommand(can::focEmergencyStopId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnEmergencyStop_InEnabled_AcksSuccess)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    DispatchCommand(can::focEmergencyStopId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnClearFault_InFault_AcksSuccess)
{
    ConstructFixtureInReady();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ResetCaptures();

    DispatchCommand(can::focClearFaultId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnStop_InReady_AcksInvalidState)
{
    ConstructFixtureInReady();
    ResetCaptures();

    DispatchCommand(can::focStopId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCanBridgeTest, OnStart_InFault_AcksInvalidState)
{
    ConstructFixtureInReady();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ResetCaptures();

    DispatchCommand(can::focStartId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidCurrent_InTorqueMode_AcksSuccess)
{
    ConstructFixture();
    ResetCaptures();

    ExpectConfigSaved();
    DispatchSetpoint(can::focSetPidCurrentId, 1000);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    EXPECT_FALSE(categoryErrorSent);
    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_NEAR(savedConfig->currentLoop.bandwidth, 1000.0f, 1e-2f);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidCurrent_AcksOnlyOnceTheSettingIsStored)
{
    ConstructFixture();
    ResetCaptures();

    infra::Function<void(services::NvmStatus)> pendingSave;
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&pendingSave](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
            {
                pendingSave = done;
            }));
    DispatchSetpoint(can::focSetPidCurrentId, 1000);
    EXPECT_FALSE(ackSpy.last.has_value());

    pendingSave(services::NvmStatus::Ok);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidCurrent_ThatCannotBeStored_IsAnsweredPersistenceFailed)
{
    ConstructFixture();
    ResetCaptures();

    ExpectConfigSaved(services::NvmStatus::WriteFailed);
    DispatchSetpoint(can::focSetPidCurrentId, 1000);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focSetPidCurrentId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::persistenceFailed);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidSpeed_InSpeedMode_AcksSuccess)
{
    ConstructFixture();
    GivenModeSelected(can::FocMotorMode::speed);
    ResetCaptures();

    ExpectConfigSaved();
    DispatchSetpoint(can::focSetPidSpeedId, 100);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    EXPECT_FALSE(categoryErrorSent);
    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_NEAR(savedConfig->speedLoop.bandwidth, 100.0f, 1e-3f);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidPosition_InPositionMode_AcksSuccess)
{
    ConstructFixture();
    GivenModeSelected(can::FocMotorMode::position);
    ResetCaptures();

    ExpectConfigSaved();
    DispatchSetpoint(can::focSetPidPositionId, 18);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    EXPECT_FALSE(categoryErrorSent);
    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_NEAR(savedConfig->positionLoop.bandwidth, 18.0f, 1e-4f);
}

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

TEST_F(FocMotorCanBridgeTest, OnSetTorqueSetpoint_InReady_AcksSuccess)
{
    ConstructFixtureInReady();
    ResetCaptures();

    DispatchSetpoint(can::focSetTorqueSetpointId, 100);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnSetSpeedSetpoint_InSpeedModeAndReady_AcksSuccess)
{
    ConstructFixtureInReady();
    GivenModeSelected(can::FocMotorMode::speed);
    AlignAfterBoot();
    ResetCaptures();

    DispatchSetpoint(can::focSetSpeedSetpointId, 100);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnSetPositionSetpoint_InPositionModeAndReady_AcksSuccess)
{
    ConstructFixtureInReady();
    GivenModeSelected(can::FocMotorMode::position);
    AlignAfterBoot();
    ResetCaptures();

    DispatchSetpoint(can::focSetPositionSetpointId, 100);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

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

TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_UnrecognisedMode_IsRejected)
{
    ConstructFixture();
    ResetCaptures();

    hal::Can::Message data;
    data.resize(2, 0);
    data[1] = 0xFF;
    EXPECT_EQ(services::CanDispatchResult::rejected, Dispatch(can::focSelectControlModeId, data));
    EXPECT_FALSE(ackSpy.last.has_value());
    EXPECT_FALSE(selectResponseSent);
}

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

TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_InvalidMode_IsRejected)
{
    ConstructFixture();
    ResetCaptures();

    hal::Can::Message data;
    data.resize(2, 0);
    data[1] = 0xFF;
    EXPECT_EQ(services::CanDispatchResult::rejected, Dispatch(can::focSelectControlModeId, data));
    EXPECT_FALSE(ackSpy.last.has_value());
    EXPECT_FALSE(selectResponseSent);
}

TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_WhenEnabled_RejectsWithCategoryError)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
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
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             infra::Function<void(std::optional<foc::Radians>)> done)
            {
                done(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> done)
            {
                EXPECT_EQ(data.stage, services::CalibrationStage::complete);
                done(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    ResetCaptures();
    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_EQ(lastSentMsgType, can::focElectricalParamsResponseId);
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    EXPECT_FALSE(categoryErrorSent);
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_AlignmentFails_SendsCalibrationFailed)
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
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             infra::Function<void(std::optional<foc::Radians>)> done)
            {
                done(std::nullopt);
            }));

    ResetCaptures();
    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::calibrationFailed);
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_CannotEnableBeforeAlignment)
{
    ConstructFixture();

    infra::Function<void(std::optional<foc::Radians>)> capturedAlignCallback;
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
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([&capturedAlignCallback](std::size_t, const auto&,
                             infra::Function<void(std::optional<foc::Radians>)> done)
            {
                capturedAlignCallback = done;
            }));

    motorServer->HandleMessage(can::focIdentifyElectricalId, SequenceOnlyPayload());
    ExecuteAllActions();

    ResetCaptures();
    DispatchCommand(can::focStartId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
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
    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::calibrationFailed);
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_FailedResistance_ReleasesTheReservation)
{
    ConstructFixture();

    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const services::ElectricalParametersIdentification::ResistanceAndInductanceConfig&,
                             infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> done)
            {
                done(services::ElectricalParametersIdentification::ResistanceInductanceResult{});
            }));

    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(coordinator->ActiveStateMachine().CurrentState()));
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_PoorFitWithAValidCalibration_ReturnsToReady)
{
    ConstructFixtureInReady();

    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const services::ElectricalParametersIdentification::ResistanceAndInductanceConfig&,
                             infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> done)
            {
                done(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 0.3f });
            }));

    ResetCaptures();
    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::calibrationFailed);
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(coordinator->ActiveStateMachine().CurrentState()));
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_NoPolePairs_ReleasesTheReservation)
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
    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(coordinator->ActiveStateMachine().CurrentState()));
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_AfterAnEmergencyStopAbandonedTheRun_IsAcceptedAgain)
{
    ConstructFixture();

    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _)).Times(2);

    DispatchCommand(can::focIdentifyElectricalId);
    DispatchCommand(can::focEmergencyStopId);

    ResetCaptures();
    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_FALSE(categoryErrorSent);
    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(coordinator->ActiveStateMachine().CurrentState()));
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_MeasuresTheOffsetsAtRestBeforeTheResistanceAndStoresThem)
{
    bridgeMeasuresOffsets = true;
    ConstructFixture();

    const foc::PhaseCurrents measured{ foc::Ampere{ 0.025f }, foc::Ampere{ -0.017f }, foc::Ampere{ 0.045f } };
    std::optional<services::CalibrationData> saved;

    InSequence order;
    EXPECT_CALL(standstillMock, Await(_)).WillOnce(Invoke([](const infra::Function<void(bool)>& done)
        {
            done(true);
        }));
    EXPECT_CALL(currentOffsetsMock, Measure(_)).WillOnce(Invoke([&measured](const infra::Function<void(std::optional<foc::PhaseCurrents>)>& done)
        {
            done(measured);
        }));
    EXPECT_CALL(currentOffsetsMock, Noise()).WillOnce(Return(foc::Ampere{ 0.0386f }));
    EXPECT_CALL(currentOffsetsMock, Apply(_)).WillOnce(Invoke([](const foc::PhaseCurrents& offsets)
        {
            EXPECT_NEAR(offsets.a.Value(), 0.025f, 1e-6f);
            EXPECT_NEAR(offsets.b.Value(), -0.017f, 1e-6f);
            EXPECT_NEAR(offsets.c.Value(), 0.045f, 1e-6f);
        }));
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
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             infra::Function<void(std::optional<foc::Radians>)> done)
            {
                done(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&saved](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> done)
            {
                saved = data;
                done(services::NvmStatus::Ok);
            }));

    ResetCaptures();
    DispatchCommand(can::focIdentifyElectricalId);

    ASSERT_TRUE(saved.has_value());
    EXPECT_NEAR(saved->currentOffsetA, 0.025f, 1e-6f);
    EXPECT_NEAR(saved->currentOffsetB, -0.017f, 1e-6f);
    EXPECT_NEAR(saved->currentOffsetC, 0.045f, 1e-6f);
    EXPECT_EQ(saved->currentNoiseMilliAmpere, 39);
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_RotorNotAtRest_FailsAndReleasesTheReservation)
{
    bridgeMeasuresOffsets = true;
    ConstructFixture();

    EXPECT_CALL(standstillMock, Await(_)).WillOnce(Invoke([](const infra::Function<void(bool)>& done)
        {
            done(false);
        }));

    ResetCaptures();
    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::calibrationFailed);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(coordinator->ActiveStateMachine().CurrentState()));
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_FailedOffsetMeasurement_FailsAndReleasesTheReservation)
{
    bridgeMeasuresOffsets = true;
    ConstructFixture();

    EXPECT_CALL(standstillMock, Await(_)).WillOnce(Invoke([](const infra::Function<void(bool)>& done)
        {
            done(true);
        }));
    EXPECT_CALL(currentOffsetsMock, Measure(_)).WillOnce(Invoke([](const infra::Function<void(std::optional<foc::PhaseCurrents>)>& done)
        {
            done(std::nullopt);
        }));

    ResetCaptures();
    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::calibrationFailed);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(coordinator->ActiveStateMachine().CurrentState()));
}

TEST_F(FocMotorCanBridgeTest, OnRequestTelemetry_InIdle_BroadcastsIdleStatus)
{
    ConstructFixture();
    ResetCaptures();

    DispatchCommand(can::focRequestTelemetryId);

    EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
    ASSERT_GE(lastSentData.size(), 2u);
    EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(can::FocMotorState::idle));
    EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::none));
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnRequestTelemetry_InPartialCalibration_BroadcastsPartialCalibrationStatus)
{
    ConstructFixtureInPartialCalibration();
    ResetCaptures();

    DispatchCommand(can::focRequestTelemetryId);

    EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
    ASSERT_GE(lastSentData.size(), 2u);
    EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(can::FocMotorState::partialCalibration));
    EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::none));
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, OnRequestTelemetry_InEnabled_BroadcastsRunningStatus)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    DispatchCommand(can::focRequestTelemetryId);

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

TEST_F(FocMotorCanBridgeTest, BroadcastFault_Stall_MapsToStall)
{
    ConstructFixture();
    ResetCaptures();

    bridge->BroadcastFault(state_machine::FaultCode::stall);

    EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
    ASSERT_GE(lastSentData.size(), 2u);
    EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::stall));
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

TEST_F(FocMotorCanBridgeTest, OnSetPidSpeed_InTorqueMode_IsRefusedAsModeMismatch)
{
    ConstructFixture();
    ResetCaptures();

    DispatchSetpoint(can::focSetPidSpeedId, 100);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focSetPidSpeedId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::modeMismatch);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidPosition_InTorqueMode_IsRefusedAsModeMismatch)
{
    ConstructFixture();
    ResetCaptures();

    DispatchSetpoint(can::focSetPidPositionId, 18);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focSetPidPositionId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::modeMismatch);
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
    DispatchCommand(can::focIdentifyElectricalId);

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

    motorServer->HandleMessage(can::focIdentifyElectricalId, SequenceOnlyPayload());

    ResetCaptures();
    DispatchCommand(can::focIdentifyElectricalId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::busy);
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_InEnabled_RejectsInvalidState)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    DispatchCommand(can::focIdentifyElectricalId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    EXPECT_FALSE(categoryErrorSent);
}

TEST_F(FocMotorCanBridgeTest, OnIdentifyElectrical_InFault_RejectsInvalidState)
{
    ConstructFixtureInReady();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ResetCaptures();

    DispatchCommand(can::focIdentifyElectricalId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    EXPECT_FALSE(categoryErrorSent);
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

    motorServer->HandleMessage(can::focSetEncoderResolutionId, []
        {
            hal::Can::Message d;
            d.resize(5, 0);
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
    DispatchCommand(can::focStartId);
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ExecuteAllActions();
    ResetCaptures();

    DispatchCommand(can::focRequestTelemetryId);

    EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
    ASSERT_GE(lastSentData.size(), 2u);
    EXPECT_EQ(lastSentData[0], static_cast<uint8_t>(can::FocMotorState::fault));
    EXPECT_EQ(lastSentData[1], static_cast<uint8_t>(can::FocFaultCode::overCurrent));
}

TEST_F(FocMotorCanBridgeTest, OnSetTorqueSetpoint_InIdle_RejectsWithInvalidState)
{
    ConstructFixture();
    ResetCaptures();

    DispatchSetpoint(can::focSetTorqueSetpointId, 100);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    EXPECT_FALSE(categoryErrorSent);
}

TEST_F(FocMotorCanBridgeTest, OnSetSpeedSetpoint_InIdle_RejectsWithInvalidState)
{
    ConstructFixture();
    GivenModeSelected(can::FocMotorMode::speed);
    ResetCaptures();

    DispatchSetpoint(can::focSetSpeedSetpointId, 50);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    EXPECT_FALSE(categoryErrorSent);
}

TEST_F(FocMotorCanBridgeTest, OnSetPositionSetpoint_InIdle_RejectsWithInvalidState)
{
    ConstructFixture();
    GivenModeSelected(can::FocMotorMode::position);
    ResetCaptures();

    DispatchSetpoint(can::focSetPositionSetpointId, 50);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    EXPECT_FALSE(categoryErrorSent);
}

TEST_F(FocMotorCanBridgeTest, OnConfigureTelemetryRate_TooHighRate_RejectsInvalidPayload)
{
    ConstructFixture();
    ResetCaptures();

    DispatchUInt32Command(can::focConfigureTelemetryRateId, 10001);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
    EXPECT_FALSE(categoryErrorSent);
}

TEST_F(FocMotorCanBridgeTest, OnConfigureTelemetryRate_WhileNvmPending_ReturnsBusy)
{
    ConstructFixture();

    infra::Function<void(services::NvmStatus)> capturedNvmCallback;
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&capturedNvmCallback](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
            {
                capturedNvmCallback = done;
            }));

    motorServer->HandleMessage(can::focConfigureTelemetryRateId, []
        {
            hal::Can::Message d;
            d.resize(5, 0);
            services::CanFrameCodec::WriteUInt32(d, 1, 100);
            return d;
        }());

    ResetCaptures();
    DispatchUInt32Command(can::focConfigureTelemetryRateId, 200);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::busy);
}

TEST_F(FocMotorCanBridgeTest, OnRequestTelemetry_InCalibrating_BroadcastsCalibratingStatus)
{
    ConstructFixture();
    ResetCaptures();

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _)).Times(AnyNumber());
    motorServer->HandleMessage(can::focRequestTelemetryId, SequenceOnlyPayload());
    ExecuteAllActions();

    EXPECT_EQ(lastSentMsgType, can::focTelemetryStatusResponseId);
}

TEST_F(FocMotorCanBridgeTest, OnSelectControlMode_AfterSuccessfulSelect_ReportsActiveMode)
{
    ConstructFixtureInReady();

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
}

TEST_F(FocMotorCanBridgeTest, Constructor_EmitsTraceMessage)
{
    EXPECT_CALL(streamWriterMock, Insert(_, _)).Times(AtLeast(1));
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .WillRepeatedly(Invoke([](infra::Function<void(bool)> done)
            {
                done(false);
            }));

    services::ConfigData config{};
    coordinator.emplace(
        application::TerminalAndTracer{ terminal, tracer },
        application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
        nvmMock,
        application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), foc::Weber{ 0.007f } },
        faultNotifierMock,
        config,
        state_machine::ControlModeStateMachine::OuterLoopArgs{
            foc::Ampere{ 10.0f },
            hal::Hertz{ 1000 },
            lowPriorityInterruptMock });

    bridge.emplace(*motorServer, *coordinator, can::FocMotorCanBridge::Platform{ inverterMock, encoderMock, diagnostics, can::FocResetCause::watchdog }, electricalIdentMock, tracer);
    motorServer->SetAcknowledger(ackSpy);
    ExecuteAllActions();
}

TEST_F(FocMotorCanBridgeTest, OnStart_WithoutCalibration_SendsCalibrationFailedCategoryError)
{
    ConstructFixture();
    ResetCaptures();

    DispatchCommand(can::focStartId);

    const bool responseSent = categoryErrorSent || ackSpy.last.has_value();
    EXPECT_TRUE(responseSent);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidCurrent_ZeroBandwidth_RejectsInvalidPayload)
{
    ConstructFixtureInReady();
    GivenModeSelected(can::FocMotorMode::torque);
    ResetCaptures();

    DispatchSetpoint(can::focSetPidCurrentId, 0);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidCurrent_NegativeBandwidth_RejectsInvalidPayload)
{
    ConstructFixtureInReady();
    GivenModeSelected(can::FocMotorMode::torque);
    ResetCaptures();

    DispatchSetpoint(can::focSetPidCurrentId, -500);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidSpeed_BandwidthAboveTheSharedLimit_RejectsInvalidPayload)
{
    ConstructFixtureInReady();
    GivenModeSelected(can::FocMotorMode::speed);
    ResetCaptures();

    DispatchSetpoint(can::focSetPidSpeedId, static_cast<int16_t>(foc::CommandLimits::maxSpeedBandwidth) + 1);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidPayload);
}

TEST_F(FocMotorCanBridgeTest, OnSetPidCurrent_WhileEnabled_RejectsInvalidState)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    DispatchSetpoint(can::focSetPidCurrentId, 1000);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCanBridgeTest, QueryAlgorithms_ReportsTheActiveModeAndEveryLoopsAlgorithm)
{
    ConstructFixture();
    ResetCaptures();

    DispatchCommand(can::focQueryAlgorithmsId);

    EXPECT_EQ(lastSentMsgType, can::focAlgorithmsResponseId);
    EXPECT_EQ(lastSentData, Bytes({ 0x00, 0x00, 0x00, 0x00 }));
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, SelectAlgorithm_InReady_SwitchesTheLoopStoresItAndReportsIt)
{
    ConstructFixtureInReady();
    ResetCaptures();

    ExpectConfigSaved();
    DispatchSelectAlgorithm(can::FocControlLoop::current, static_cast<uint8_t>(can::FocCurrentAlgorithm::decoupledPid));

    EXPECT_EQ(coordinator->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::decoupledPid);
    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_EQ(savedConfig->currentAlgorithm, static_cast<uint8_t>(foc::CurrentAlgorithm::decoupledPid));
    EXPECT_EQ(lastSentMsgType, can::focAlgorithmsResponseId);
    EXPECT_EQ(lastSentData, Bytes({ 0x00, 0x01, 0x00, 0x00 }));
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, SelectAlgorithm_NeedingAMotorModel_IsRefusedWithInvalidParametersBeforeIdentification)
{
    ConstructFixture();
    ResetCaptures();

    DispatchSelectAlgorithm(can::FocControlLoop::current, static_cast<uint8_t>(can::FocCurrentAlgorithm::deadbeat));

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focSelectAlgorithmId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::invalidParameters);
    EXPECT_EQ(coordinator->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(FocMotorCanBridgeTest, SelectAlgorithm_ForALoopTheModeDoesNotRun_IsAModeMismatch)
{
    ConstructFixture();
    ResetCaptures();

    DispatchSelectAlgorithm(can::FocControlLoop::position, static_cast<uint8_t>(can::FocPositionAlgorithm::cascadeP));

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::modeMismatch);
}

TEST_F(FocMotorCanBridgeTest, SelectAlgorithm_WhileEnabled_IsAnInvalidState)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    DispatchSelectAlgorithm(can::FocControlLoop::current, static_cast<uint8_t>(can::FocCurrentAlgorithm::slidingMode));

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    EXPECT_EQ(coordinator->ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(FocMotorCanBridgeTest, QueryParameter_AnUntouchedBandwidth_ReportsTheDerivedValueAsAutomatic)
{
    ConstructFixture();
    ResetCaptures();

    DispatchQueryParameter(can::FocParameter::currentBandwidth);

    const auto answer = AnsweredParameter();
    EXPECT_TRUE(answer.automatic);
    EXPECT_NEAR(answer.value, 10000.0f / 15.0f * 2.0f * std::numbers::pi_v<float>, 0.01f);
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, QueryParameter_ALoopTheModeDoesNotRun_ReportsItsStoredSetting)
{
    ConstructFixture();
    ResetCaptures();

    DispatchQueryParameter(can::FocParameter::speedBandwidth);
    const auto bandwidth = AnsweredParameter();
    EXPECT_TRUE(bandwidth.automatic);
    EXPECT_NEAR(bandwidth.value, 0.0f, 1e-9f);

    DispatchQueryParameter(can::FocParameter::speedObserverBandwidthRatio);
    ExpectAnswered(foc::SpeedLoopTunings{}.observerBandwidthRatio, false);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_ALoopSetting_IsAppliedStoredAndAnswered)
{
    ConstructFixture();
    ResetCaptures();

    ExpectConfigSaved();
    DispatchSetParameter(can::FocParameter::currentBoundaryLayer, 0.8f);

    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_NEAR(savedConfig->currentLoop.boundaryLayer, 0.8f, 1e-6f);
    ExpectAnswered(0.8f, false);
    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_ABandwidthSetToZero_ReturnsToAutomatic)
{
    ConstructFixture();
    ExpectConfigSaved();
    DispatchSetParameter(can::FocParameter::currentBandwidth, 5000.0f);
    ExpectAnswered(5000.0f, false);

    ResetCaptures();
    ExpectConfigSaved();
    DispatchSetParameter(can::FocParameter::currentBandwidth, 0.0f);

    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_NEAR(savedConfig->currentLoop.bandwidth, 0.0f, 1e-9f);
    const auto answer = AnsweredParameter();
    EXPECT_TRUE(answer.automatic);
    EXPECT_NEAR(answer.value, 10000.0f / 15.0f * 2.0f * std::numbers::pi_v<float>, 0.01f);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_ASwitchingGainReachingTheBoundaryLayer_IsRefusedAndNotStored)
{
    ConstructFixture();
    ResetCaptures();

    DispatchSetParameter(can::FocParameter::currentSwitchingGain, 0.5f);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focSetParameterId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::invalidParameters);
    EXPECT_FALSE(savedConfig.has_value());
    EXPECT_NEAR(coordinator->StoredCurrentTunings().switchingGain, foc::CurrentLoopTunings{}.switchingGain, 1e-6f);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_ASpeedSettingInTorqueMode_IsAModeMismatch)
{
    ConstructFixture();
    ResetCaptures();

    DispatchSetParameter(can::FocParameter::speedObserverBandwidthRatio, 8.0f);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::modeMismatch);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_WhileEnabled_IsAnInvalidState)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    DispatchSetParameter(can::FocParameter::currentBoundaryLayer, 0.8f);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_ThatCannotBeStored_ReportsPersistenceFailed)
{
    ConstructFixture();
    ResetCaptures();

    ExpectConfigSaved(services::NvmStatus::WriteFailed);
    DispatchSetParameter(can::FocParameter::currentBoundaryLayer, 0.8f);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::persistenceFailed);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_AMotorParameter_IsWrittenToTheCalibrationRecord)
{
    ConstructFixtureInReady();
    ResetCaptures();

    ExpectCalibrationSaved();
    DispatchSetParameter(can::FocParameter::resistance, 0.36f);

    ASSERT_TRUE(savedCalibration.has_value());
    EXPECT_NEAR(savedCalibration->rPhase, 0.36f, 1e-6f);
    EXPECT_EQ(savedCalibration->stage, services::CalibrationStage::complete);
    ExpectAnswered(0.36f, false);
    EXPECT_NEAR(coordinator->CalibrationRecord().rPhase, 0.36f, 1e-6f);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_PolePairs_RequiresAnotherAlignmentBeforeStart)
{
    ConstructFixtureInReady();
    ResetCaptures();

    ExpectCalibrationSaved();
    DispatchSetParameter(can::FocParameter::polePairs, 7.0f);
    ExpectAnswered(7.0f, false);

    ResetCaptures();
    DispatchCommand(can::focStartId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(coordinator->ActiveStateMachine().CurrentState()));
}

TEST_F(FocMotorCanBridgeTest, QueryParameter_AnUnsetFluxLinkage_ReportsThePlatformDefaultAsAutomatic)
{
    ConstructFixtureInReady();
    ResetCaptures();

    DispatchQueryParameter(can::FocParameter::fluxLinkage);

    ExpectAnswered(0.007f, true);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_AStartupSetting_IsStoredForTheNextStart)
{
    ConstructFixture();
    ResetCaptures();

    ExpectConfigSaved();
    DispatchSetParameter(can::FocParameter::canBitrate, 500000.0f);

    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_EQ(savedConfig->canBaudrate, 500000u);
    ExpectAnswered(500000.0f, false);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_AStartupSettingThatCannotBeStored_IsTakenBack)
{
    ConstructFixture();
    ResetCaptures();

    ExpectConfigSaved(services::NvmStatus::WriteFailed);
    DispatchSetParameter(can::FocParameter::canNodeId, 9.0f);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::persistenceFailed);
    EXPECT_EQ(coordinator->Config().canNodeId, 1u);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_ASupervisionSetting_IsStoredAndInForce)
{
    ConstructFixture();
    ResetCaptures();

    ExpectConfigSaved();
    DispatchSetParameter(can::FocParameter::supervisionWindow, 2.5f);

    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_NEAR(savedConfig->supervision.window, 2.5f, 1e-6f);
    EXPECT_NEAR(coordinator->Config().supervision.window, 2.5f, 1e-6f);
    ExpectAnswered(2.5f, false);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_StallDetectionOff_IsAcceptedWhileTheDriveRuns)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    ExpectConfigSaved();
    DispatchSetParameter(can::FocParameter::stallDetection, 0.0f);

    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_EQ(savedConfig->stallDetection, uint8_t{ 0 });
    ExpectAnswered(0.0f, false);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_ASupervisionSettingThatCannotBeStored_IsTakenBack)
{
    ConstructFixture();
    ResetCaptures();

    ExpectConfigSaved(services::NvmStatus::WriteFailed);
    DispatchSetParameter(can::FocParameter::supervisionPositionError, 1.0f);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::persistenceFailed);
    EXPECT_NEAR(coordinator->Config().supervision.positionError, services::SupervisionSettings{}.positionError, 1e-6f);
}

TEST_F(FocMotorCanBridgeTest, QueryParameter_ASupervisionSetting_AnswersTheStoredValue)
{
    ConstructFixture();
    ResetCaptures();

    DispatchQueryParameter(can::FocParameter::supervisionStillExcursion);

    ExpectAnswered(services::SupervisionSettings{}.stillExcursion, false);
}

TEST_F(FocMotorCanBridgeTest, SetParameter_WhileAnotherWriteIsOutstanding_IsBusy)
{
    ConstructFixture();
    ResetCaptures();

    infra::Function<void(services::NvmStatus)> pendingSave;
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&pendingSave](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
            {
                pendingSave = done;
            }));
    DispatchSetParameter(can::FocParameter::currentBoundaryLayer, 0.8f);
    DispatchSetParameter(can::FocParameter::currentSwitchingGain, 0.1f);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focSetParameterId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::busy);

    pendingSave(services::NvmStatus::Ok);
    ExecuteAllActions();
}

TEST_F(FocMotorCanBridgeTest, AnEncoderResolutionWrite_KeepsAnAlgorithmSelectedBeforeIt)
{
    ConstructFixtureInReady();
    ExpectConfigSaved();
    DispatchSelectAlgorithm(can::FocControlLoop::current, static_cast<uint8_t>(can::FocCurrentAlgorithm::slidingMode));

    ExpectConfigSaved();
    DispatchUInt32Command(can::focSetEncoderResolutionId, 8192);

    ASSERT_TRUE(savedConfig.has_value());
    EXPECT_EQ(savedConfig->encoderResolution, 8192u);
    EXPECT_EQ(savedConfig->currentAlgorithm, static_cast<uint8_t>(foc::CurrentAlgorithm::slidingMode));
}

TEST_F(FocMotorCanBridgeTest, Calibrate_WhileEnabled_IsAnInvalidState)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    DispatchCommand(can::focCalibrateId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focCalibrateId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCanBridgeTest, Calibrate_WhileARunIsInProgress_IsBusy)
{
    ConstructFixture();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _));
    DispatchCommand(can::focCalibrateId);
    ResetCaptures();

    DispatchCommand(can::focCalibrateId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focCalibrateId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::busy);
    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(coordinator->ActiveStateMachine().CurrentState()));
}

TEST_F(FocMotorCanBridgeTest, Calibrate_ThatFails_ReportsCalibrationFailed)
{
    ConstructFixture();
    infra::Function<void(std::optional<std::size_t>)> polePairsDone;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([&polePairsDone](const auto&, const infra::Function<void(std::optional<std::size_t>)>& onDone)
            {
                polePairsDone = onDone;
            }));
    DispatchCommand(can::focCalibrateId);
    ResetCaptures();

    polePairsDone(std::nullopt);
    ExecuteAllActions();

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focCalibrateId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::calibrationFailed);
}

TEST_F(FocMotorCanBridgeTest, ClearCalibration_InReady_DropsTheRecordAndReturnsToIdle)
{
    ConstructFixtureInReady();
    ResetCaptures();
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> done)
            {
                done(services::NvmStatus::Ok);
            }));

    DispatchCommand(can::focClearCalibrationId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focClearCalibrationId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(coordinator->ActiveStateMachine().CurrentState()));
}

TEST_F(FocMotorCanBridgeTest, ClearCalibration_ThatCannotBeStored_ReportsPersistenceFailed)
{
    ConstructFixtureInReady();
    ResetCaptures();
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> done)
            {
                done(services::NvmStatus::WriteFailed);
            }));

    DispatchCommand(can::focClearCalibrationId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focClearCalibrationId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::persistenceFailed);
}

TEST_F(FocMotorCanBridgeTest, ClearCalibration_WhileEnabled_IsAnInvalidState)
{
    ConstructFixtureInReady();
    DispatchCommand(can::focStartId);
    ResetCaptures();

    DispatchCommand(can::focClearCalibrationId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focClearCalibrationId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::invalidState);
}

TEST_F(FocMotorCanBridgeTest, ClearDiagnostics_StartsTheLoopStatisticsAfresh)
{
    ConstructFixture();
    loopMetrics.Record(1200);
    loopMetrics.RecordReentry();
    ResetCaptures();

    DispatchCommand(can::focClearDiagnosticsId);

    ASSERT_TRUE(ackSpy.last.has_value());
    EXPECT_EQ(ackSpy.last->commandType, can::focClearDiagnosticsId);
    EXPECT_EQ(ackSpy.last->status, services::CanAckStatus::success);
    EXPECT_EQ(loopMetrics.Read().samples, 0u);
    EXPECT_EQ(loopMetrics.Read().reentries, 0u);
}

TEST_F(FocMotorCanBridgeTest, QueryParameter_LoopStatistics_ReportTheMeasuredCycles)
{
    ConstructFixture();
    loopMetrics.Configure(4500, 6000);
    loopMetrics.Record(1000);
    loopMetrics.Record(5000);
    loopMetrics.RecordReentry();
    ResetCaptures();

    DispatchQueryParameter(can::FocParameter::loopSamples);
    ExpectAnswered(2.0f, false);
    DispatchQueryParameter(can::FocParameter::loopBudgetCycles);
    ExpectAnswered(4500.0f, false);
    DispatchQueryParameter(can::FocParameter::loopMinimumCycles);
    ExpectAnswered(1000.0f, false);
    DispatchQueryParameter(can::FocParameter::loopAverageCycles);
    ExpectAnswered(static_cast<float>(loopMetrics.Read().averageCycles), false);
    DispatchQueryParameter(can::FocParameter::loopMaximumCycles);
    ExpectAnswered(5000.0f, false);
    DispatchQueryParameter(can::FocParameter::loopOverruns);
    ExpectAnswered(1.0f, false);
    DispatchQueryParameter(can::FocParameter::loopDeadlineMisses);
    ExpectAnswered(0.0f, false);
    DispatchQueryParameter(can::FocParameter::loopReentries);
    ExpectAnswered(1.0f, false);
}

TEST_F(FocMotorCanBridgeTest, QueryParameter_ResetCause_ReportsTheCauseOfTheLastReset)
{
    ConstructFixture();
    ResetCaptures();

    DispatchQueryParameter(can::FocParameter::resetCause);

    ExpectAnswered(static_cast<float>(can::FocResetCause::watchdog), false);
}

TEST_F(FocMotorCanBridgeTest, QueryParameter_CalibrationReadings_ReportTheStoredRecord)
{
    EXPECT_CALL(nvmMock, IsCalibrationValid(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke([](infra::Function<void(bool)> done)
            {
                done(true);
            }));
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke([](services::CalibrationData& data, infra::Function<void(services::NvmStatus)> done)
            {
                data = services::CalibrationData{};
                data.polePairs = 4;
                data.rPhase = 0.5f;
                data.lD = 1.0f;
                data.lQ = 1.0f;
                data.currentOffsetA = 0.012f;
                data.currentOffsetB = -0.004f;
                data.currentOffsetC = 0.002f;
                data.coggingTorque.fill(100);
                data.coggingTorqueScale = 0.001f;
                data.stage = services::CalibrationStage::complete;
                done(services::NvmStatus::Ok);
            }));
    Construct();
    ResetCaptures();

    DispatchQueryParameter(can::FocParameter::currentOffsetA);
    ExpectAnswered(0.012f, false);
    DispatchQueryParameter(can::FocParameter::currentOffsetB);
    ExpectAnswered(-0.004f, false);
    DispatchQueryParameter(can::FocParameter::currentOffsetC);
    ExpectAnswered(0.002f, false);
    DispatchQueryParameter(can::FocParameter::coggingPeak);
    ExpectAnswered(0.1f, false);
}

TEST_F(FocMotorCanBridgeTest, QueryParameter_OnlineEstimates_InTorqueMode_AreZero)
{
    ConstructFixtureInReady();
    ResetCaptures();

    DispatchQueryParameter(can::FocParameter::estimatedResistance);
    ExpectAnswered(0.0f, false);
    DispatchQueryParameter(can::FocParameter::estimatedViscousFriction);
    ExpectAnswered(0.0f, false);
}

TEST_F(FocMotorCanBridgeTest, QueryParameter_OnlineEstimates_InSpeedMode_ReportTheEstimators)
{
    ConstructFixture();
    GivenModeSelected(can::FocMotorMode::speed);
    ResetCaptures();

    const auto estimates = coordinator->ActiveStateMachine().CurrentOnlineEstimates();
    ASSERT_TRUE(estimates.has_value());

    DispatchQueryParameter(can::FocParameter::estimatedResistance);
    ExpectAnswered(estimates->resistance.Value(), false);
    DispatchQueryParameter(can::FocParameter::estimatedInductance);
    ExpectAnswered(estimates->inductance.Value(), false);
    DispatchQueryParameter(can::FocParameter::estimatedInertia);
    ExpectAnswered(estimates->inertia.Value(), false);
    DispatchQueryParameter(can::FocParameter::estimatedViscousFriction);
    ExpectAnswered(estimates->viscousFriction.Value(), false);
}

TEST_F(FocMotorCanBridgeTest, ClearCalibration_WhileASettingIsBeingStored_IsBusy)
{
    ConstructFixture();
    infra::Function<void(services::NvmStatus)> pendingSave;
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&pendingSave](const services::ConfigData&, infra::Function<void(services::NvmStatus)> done)
            {
                pendingSave = done;
            }));
    DispatchSetParameter(can::FocParameter::currentBoundaryLayer, 0.8f);
    ResetCaptures();

    DispatchCommand(can::focClearCalibrationId);

    EXPECT_TRUE(categoryErrorSent);
    EXPECT_EQ(lastCategoryErrorOriginCmd, can::focClearCalibrationId);
    EXPECT_EQ(lastCategoryError, can::FocMotorCategoryError::busy);

    pendingSave(services::NvmStatus::Ok);
    ExecuteAllActions();
}
