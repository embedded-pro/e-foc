#pragma once

#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "core/can/FocMotorCanBridge.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/Function.hpp"
#include "integration_tests/software_in_the_loop/support/EepromStub.hpp"
#include "integration_tests/software_in_the_loop/support/PlatformFactoryMock.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <gmock/gmock.h>
#include <optional>

namespace sil
{
    struct ControlModeCoordinationFixture
        : infra::ClockFixture
    {
        using CoordinatorType = state_machine::ControlModeStateMachine;
        using BridgeType = can::FocMotorCanBridge;

        ControlModeCoordinationFixture();

        void ConstructCoordinator(services::ConfigData data = services::MakeDefaultConfigData());
        void RestartCoordinator();

        void SetupCanIntegration();
        void InjectCanStart();
        void InjectCanStop();
        void InjectCanSelectControlMode(can::FocMotorMode mode);
        void InjectCanSetTorqueSetpoint(int16_t value);
        void InjectCanSetSpeedSetpoint(int16_t value);
        void InjectCanSetPositionSetpoint(int16_t value);

        void DispatchToMotor(uint8_t messageType, const hal::Can::Message& data);

        static services::CalibrationData MakeDefaultCalibrationData();

        static const foc::Volts testVdc;

        testing::StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy tracerStream{ streamWriterMock };
        services::TracerToStream tracer{ tracerStream };
        testing::StrictMock<hal::SerialCommunicationMock> serialCommunication;
        infra::Execute setupInfraExpectations{ [this]()
            {
                using namespace testing;
                EXPECT_CALL(streamWriterMock, Insert(_, _)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Available()).Times(AnyNumber()).WillRepeatedly(Return(1000));
                EXPECT_CALL(streamWriterMock, ConstructSaveMarker()).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, GetProcessedBytesSince(_)).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, SaveState(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(streamWriterMock, RestoreState(_)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Overwrite(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(serialCommunication, SendDataMock(_)).Times(AnyNumber());
            } };
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ serialCommunication, tracer };
        services::TerminalWithStorage::WithMaxSize<20> terminal{ terminalWithCommands, tracer };
        std::optional<services::TerminalWithStorage::WithMaxSize<20>> terminalAfterRestart;

        testing::StrictMock<PlatformFactoryMock> platformFactory;
        EepromStub eepromStub;

        services::NvmEepromRegion calibrationRegion{ eepromStub, 0, 128 };
        services::NvmEepromRegion configRegion{ eepromStub, 128, 128 };
        services::NonVolatileMemoryImpl nvm{ calibrationRegion, configRegion };

        testing::StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        testing::StrictMock<services::MotorAlignmentMock> alignmentMock;
        testing::StrictMock<services::MechanicalParametersIdentificationMock> mechIdentMock;
        testing::StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        testing::StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;
        infra::Execute setupTeardownExpectations{ [this]()
            {
                using namespace testing;
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
            } };

        std::optional<CoordinatorType> coordinator;

        uint8_t lastSentMessageType{ 0 };
        bool commandAckSent{ false };
        bool selectResponseSent{ false };
        services::CanAckStatus lastCommandAckStatus{ services::CanAckStatus::success };
        uint8_t lastCommandAckMessageType{ 0 };
        bool categoryErrorSent{ false };
        uint8_t lastCategoryErrorOriginCmd{ 0 };
        can::FocMotorCategoryError lastCategoryErrorReason{ can::FocMotorCategoryError::busy };

        testing::StrictMock<hal::CanMock> transportCanMock;
        std::optional<services::CanProtocolServer> canProtocolServer;
        std::optional<can::FocMotorCategoryServer> motorCategoryServer;
        std::optional<BridgeType> motorBridge;
    };
}
