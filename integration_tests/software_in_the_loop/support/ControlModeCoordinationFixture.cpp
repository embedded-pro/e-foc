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
            application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
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
            application::CalibrationServices{ electricalIdentMock, alignmentMock, &mechIdentMock },
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
        commandRejectedSent = false;
        selectResponseSent = false;
        lastSentMessageType = 0;
        lastSelectResponseReason = services::FocRejectReason::ok;
        lastCommandRejectedReason = services::FocRejectReason::ok;
        lastCommandRejectedOrigCmdId = 0;

        EXPECT_CALL(transportCanMock, SendData(_, _, _))
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

        canTransport.emplace(transportCanMock, 1);
        motorCategoryServer.emplace(*canTransport);
        motorBridge.emplace(*motorCategoryServer, *coordinator);
    }

    void ControlModeCoordinationFixture::InjectCanStart()
    {
        commandRejectedSent = false;
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(services::focStartId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanStop()
    {
        hal::Can::Message data;
        data.push_back(0x01);
        motorCategoryServer->HandleMessage(services::focStopId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanSelectControlMode(services::FocMotorMode mode)
    {
        commandRejectedSent = false;
        selectResponseSent = false;
        lastSelectResponseReason = services::FocRejectReason::ok;

        hal::Can::Message data;
        data.resize(2, 0);
        data[0] = 0;
        data[1] = static_cast<uint8_t>(mode);
        motorCategoryServer->HandleMessage(services::focSelectControlModeId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanSetTorqueSetpoint(int16_t value)
    {
        commandRejectedSent = false;
        lastCommandRejectedReason = services::FocRejectReason::ok;
        lastCommandRejectedOrigCmdId = 0;
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = 0;
        services::CanFrameCodec::WriteInt16(data, 1, value);
        motorCategoryServer->HandleMessage(services::focSetTorqueSetpointId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanSetSpeedSetpoint(int16_t value)
    {
        commandRejectedSent = false;
        lastCommandRejectedReason = services::FocRejectReason::ok;
        lastCommandRejectedOrigCmdId = 0;
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = 0;
        services::CanFrameCodec::WriteInt16(data, 1, value);
        motorCategoryServer->HandleMessage(services::focSetSpeedSetpointId, data);
        ExecuteAllActions();
    }

    void ControlModeCoordinationFixture::InjectCanSetPositionSetpoint(int16_t value)
    {
        commandRejectedSent = false;
        lastCommandRejectedReason = services::FocRejectReason::ok;
        lastCommandRejectedOrigCmdId = 0;
        hal::Can::Message data;
        data.resize(3, 0);
        data[0] = 0;
        services::CanFrameCodec::WriteInt16(data, 1, value);
        motorCategoryServer->HandleMessage(services::focSetPositionSetpointId, data);
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
