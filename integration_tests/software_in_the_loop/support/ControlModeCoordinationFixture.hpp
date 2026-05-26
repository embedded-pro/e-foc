#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/foc/implementations/FocPositionImpl.hpp"
#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/foc/implementations/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/FocMotorCanBridge.hpp"
#include "core/state_machine/FocStateMachineImpl.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/util/Function.hpp"
#include "integration_tests/software_in_the_loop/support/EepromStub.hpp"
#include "integration_tests/software_in_the_loop/support/PlatformFactoryMock.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <gmock/gmock.h>
#include <optional>

namespace integration
{
    struct ControlModeCoordinationFixture
        : infra::EventDispatcherWithWeakPtrFixture
    {
        using CoordinatorType = state_machine::ControlModeStateMachine<
            foc::FocTorqueImpl,
            foc::FocSpeedImpl,
            foc::FocPositionImpl>;

        using BridgeType = state_machine::FocMotorCanBridge<
            foc::FocTorqueImpl,
            foc::FocSpeedImpl,
            foc::FocPositionImpl>;

        ControlModeCoordinationFixture();

        void ConstructCoordinator(services::ConfigData data = services::MakeDefaultConfigData());
        void RestartCoordinator();

        void SetupCanIntegration();
        void InjectCanStart();
        void InjectCanStop();
        void InjectCanSelectControlMode(services::FocMotorMode mode);
        void InjectCanSetTorqueSetpoint(int16_t value);
        void InjectCanSetSpeedSetpoint(int16_t value);
        void InjectCanSetPositionSetpoint(int16_t value);

        static services::CalibrationData MakeDefaultCalibrationData();

        static const foc::Volts testVdc;

        testing::StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy tracerStream{ streamWriterMock };
        services::TracerToStream tracer{ tracerStream };
        hal::SerialCommunicationMock serialCommunication;
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
        // A fresh command registry used after RestartCoordinator() to simulate a
        // power cycle where the terminal is also re-initialised.
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

        std::optional<CoordinatorType> coordinator;

        int nvmSaveCount{ 0 };
        uint8_t lastSentMessageType{ 0 };
        bool commandRejectedSent{ false };
        bool selectResponseSent{ false };
        services::FocRejectReason lastSelectResponseReason{ services::FocRejectReason::ok };
        services::FocRejectReason lastCommandRejectedReason{ services::FocRejectReason::ok };
        uint8_t lastCommandRejectedOrigCmdId{ 0 };

        testing::StrictMock<hal::CanMock> transportCanMock;
        std::optional<services::CanFrameTransport> canTransport;
        std::optional<services::FocMotorCategoryServer> motorCategoryServer;
        std::optional<BridgeType> motorBridge;
    };
}
