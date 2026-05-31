#include "integration_tests/software_in_the_loop/support/ControlModeCoordinationFixture.hpp"
#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"

namespace integration
{
    const foc::Volts ControlModeCoordinationFixture::testVdc{ 24.0f };
    using namespace testing;

    ControlModeCoordinationFixture::ControlModeCoordinationFixture()
    {
        EXPECT_CALL(platformFactory, PhaseCurrentsReady(_, _)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, ThreePhasePwmOutput(_)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, BaseFrequency()).Times(AnyNumber()).WillRepeatedly(Return(hal::Hertz{ 10000 }));
        EXPECT_CALL(platformFactory, Start()).Times(AnyNumber());
        EXPECT_CALL(platformFactory, Stop()).Times(AnyNumber());
        EXPECT_CALL(platformFactory, Read()).Times(AnyNumber()).WillRepeatedly(Return(foc::Radians{ 0.0f }));
        EXPECT_CALL(platformFactory, Set(_)).Times(AnyNumber());
        EXPECT_CALL(platformFactory, SetZero()).Times(AnyNumber());
        EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
    }

    void ControlModeCoordinationFixture::ConstructCoordinator(services::ConfigData data)
    {
        EXPECT_CALL(faultNotifierMock, Register(_))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& handler)
                {
                    faultNotifierMock.StoreHandler(handler);
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

        // Simulate a full power cycle: the terminal command registry is also fresh.
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
                    if (lastSentMessageType == services::focSelectControlModeResponseId)
                        selectResponseSent = true;
                    if (lastSentMessageType == services::focCategoryErrorResponseId && msg.size() >= 2)
                    {
                        categoryErrorSent = true;
                        lastCategoryErrorOriginCmd = msg[0];
                        lastCategoryErrorReason = static_cast<services::FocMotorCategoryError>(msg[1]);
                    }
                    cb(true);
                }));

        canTransport.emplace(transportCanMock, 1);
        motorCategoryServer.emplace(*canTransport);
        motorCategoryServer->SetAcknowledger(*this);
        motorBridge.emplace(*motorCategoryServer, *coordinator);
    }

    void ControlModeCoordinationFixture::SendCommandAck(uint8_t /*categoryId*/, uint8_t commandType, services::CanAckStatus status)
    {
        commandAckSent = true;
        lastCommandAckMessageType = commandType;
        lastCommandAckStatus = status;
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
        DispatchToMotor(services::focStartId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanStop()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        DispatchToMotor(services::focStopId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanSelectControlMode(services::FocMotorMode mode)
    {
        commandAckSent = false;
        selectResponseSent = false;
        lastCommandAckStatus = services::CanAckStatus::success;

        hal::Can::Message data;
        data.resize(2, 0);
        data[0] = 0;
        data[1] = static_cast<uint8_t>(mode);
        DispatchToMotor(services::focSelectControlModeId, data);
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
        DispatchToMotor(services::focSetTorqueSetpointId, data);
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
        DispatchToMotor(services::focSetSpeedSetpointId, data);
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
        DispatchToMotor(services::focSetPositionSetpointId, data);
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
