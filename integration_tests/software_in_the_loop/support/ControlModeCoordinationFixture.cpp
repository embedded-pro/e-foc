#include "integration_tests/software_in_the_loop/support/ControlModeCoordinationFixture.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"

namespace sil
{
    const foc::Volts ControlModeCoordinationFixture::testVdc{ 24.0f };
    using namespace testing;

    ControlModeCoordinationFixture::ControlModeCoordinationFixture()
    {
        EXPECT_CALL(platformFactory, PhaseCurrentsReady(_, _)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, ThreePhasePwmOutput(_)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, BaseFrequency()).Times(AnyNumber()).WillRepeatedly(Return(hal::Hertz{ 20000 }));
        EXPECT_CALL(platformFactory, MaxCurrentSupported()).Times(AnyNumber()).WillRepeatedly(Return(foc::Ampere{ 15.0f }));
        EXPECT_CALL(platformFactory, Start()).Times(AnyNumber());
        EXPECT_CALL(platformFactory, Stop()).Times(AnyNumber());
        EXPECT_CALL(platformFactory, Read()).Times(AnyNumber()).WillRepeatedly(Return(foc::Radians{ 0.0f }));
        EXPECT_CALL(platformFactory, Set(_)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, SetZero()).Times(AnyNumber());
        EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
        EXPECT_CALL(lowPriorityInterruptMock, Unregister()).Times(AnyNumber());
    }

    void ControlModeCoordinationFixture::ConstructCoordinator(services::ConfigData data)
    {
        EXPECT_CALL(faultNotifierMock, Register(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& immediate, const infra::Function<void(state_machine::FaultCode)>& deferred)
                {
                    faultNotifierMock.StoreHandler(immediate, deferred);
                }));

        bool saved = false;
        nvm.SaveCalibration(MakeDefaultCalibrationData(), [&saved](services::NvmStatus)
            {
                saved = true;
            });
        ExecuteAllActions();
        EXPECT_TRUE(saved);

        coordinator.emplace(
            application::TerminalAndTracer{ terminal, tracer },
            application::MotorHardware{ platformFactory, platformFactory, testVdc },
            nvm,
            application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
            faultNotifierMock,
            data,
            CoordinatorType::OuterLoopArgs{
                foc::Ampere{ 10.0f },
                hal::Hertz{ 1000 },
                lowPriorityInterruptMock });

        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::RestartCoordinator()
    {
        motorBridge.reset();
        coordinator.reset();

        terminalAfterRestart.emplace(terminalWithCommands, tracer);
        services::TerminalWithStorage& freshTerminal = *terminalAfterRestart;

        services::ConfigData savedData;
        bool loaded = false;
        nvm.LoadConfig(savedData, [&loaded](services::NvmStatus status)
            {
                if (status == services::NvmStatus::Ok)
                    loaded = true;
            });
        ExecuteAllActions();

        if (!loaded)
            savedData = services::MakeDefaultConfigData();

        coordinator.emplace(
            application::TerminalAndTracer{ freshTerminal, tracer },
            application::MotorHardware{ platformFactory, platformFactory, testVdc },
            nvm,
            application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
            faultNotifierMock,
            savedData,
            CoordinatorType::OuterLoopArgs{
                foc::Ampere{ 10.0f },
                hal::Hertz{ 1000 },
                lowPriorityInterruptMock });

        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::SetupCanIntegration()
    {
        commandAckSent = false;
        selectResponseSent = false;
        lastSentMessageType = 0;
        lastCommandAckStatus = services::CanAckStatus::success;
        lastCommandAckMessageType = 0;
        categoryErrorSent = false;

        EXPECT_CALL(transportCanMock, SendData(_, _, _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this](hal::Can::Id id, const hal::Can::Message& msg, const infra::Function<void(bool)>& cb)
                {
                    lastSentMessageType = services::ExtractCanMessageType(id.Get29BitId());
                    if (lastSentMessageType == can::focSelectControlModeResponseId)
                        selectResponseSent = true;
                    if (lastSentMessageType == services::canCategoryErrorResponseMessageTypeId && msg.size() >= 2)
                    {
                        categoryErrorSent = true;
                        lastCategoryErrorOriginCmd = msg[0];
                        lastCategoryErrorReason = static_cast<can::FocMotorCategoryError>(msg[1]);
                    }
                    if (lastSentMessageType == services::canCommandAckMessageTypeId && msg.size() >= services::canCommandAckSize)
                    {
                        commandAckSent = true;
                        lastCommandAckMessageType = msg[1];
                        lastCommandAckStatus = static_cast<services::CanAckStatus>(msg[2]);
                    }
                    cb(true);
                }));
        EXPECT_CALL(transportCanMock, ReceiveData(_)).Times(AnyNumber());

        canProtocolServer.emplace(transportCanMock, services::CanProtocolServer::Config{ .nodeId = 1 });
        motorCategoryServer.emplace(canProtocolServer->Transport());
        canProtocolServer->RegisterCategory(*motorCategoryServer);
        motorBridge.emplace(*motorCategoryServer, *coordinator, platformFactory, electricalIdentMock, &mechIdentMock, foc::NewtonMeter{ 0.1f }, nvm, services::MakeDefaultConfigData(), tracer);
    }

    void ControlModeCoordinationFixture::DispatchToMotor(uint8_t messageType, const hal::Can::Message& data)
    {
        motorCategoryServer->HandleMessage(messageType, data);
    }

    void ControlModeCoordinationFixture::InjectCanStart()
    {
        commandAckSent = false;
        hal::Can::Message data;
        data.push_back(0x01);
        DispatchToMotor(can::focStartId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanStop()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        DispatchToMotor(can::focStopId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanSelectControlMode(can::FocMotorMode mode)
    {
        commandAckSent = false;
        selectResponseSent = false;
        lastCommandAckStatus = services::CanAckStatus::success;

        hal::Can::Message data;
        data.resize(2, 0);
        data[0] = 0;
        data[1] = static_cast<uint8_t>(mode);
        DispatchToMotor(can::focSelectControlModeId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanSetTorqueSetpoint(int16_t value)
    {
        commandAckSent = false;
        lastCommandAckStatus = services::CanAckStatus::success;
        lastCommandAckMessageType = 0;
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = 0;
        services::CanFrameCodec::WriteInt16(data, 1, value);
        DispatchToMotor(can::focSetTorqueSetpointId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanSetSpeedSetpoint(int16_t value)
    {
        commandAckSent = false;
        lastCommandAckStatus = services::CanAckStatus::success;
        lastCommandAckMessageType = 0;
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = 0;
        services::CanFrameCodec::WriteInt16(data, 1, value);
        DispatchToMotor(can::focSetSpeedSetpointId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanSetPositionSetpoint(int16_t value)
    {
        commandAckSent = false;
        lastCommandAckStatus = services::CanAckStatus::success;
        lastCommandAckMessageType = 0;
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = 0;
        services::CanFrameCodec::WriteInt16(data, 1, value);
        DispatchToMotor(can::focSetPositionSetpointId, data);
        ExecuteAllActions();
    }

    services::CalibrationData ControlModeCoordinationFixture::MakeDefaultCalibrationData()
    {
        services::CalibrationData data{};
        data.polePairs = 4;
        data.rPhase = 0.5f;
        return data;
    }
}
