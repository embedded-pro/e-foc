#pragma once

#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "core/can/FocMotorCategoryServer.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/state_machine/TorqueStateMachine.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/Function.hpp"
#include "integration_tests/software_in_the_loop/support/EepromStub.hpp"
#include "integration_tests/software_in_the_loop/support/FocMotorStateMachineBridge.hpp"
#include "integration_tests/software_in_the_loop/support/PlatformFactoryMock.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <gmock/gmock.h>
#include <optional>

namespace sil
{
    struct FocIntegrationFixture
        : infra::ClockFixture
    {
        FocIntegrationFixture();

        void ConstructWithInvalidNvm();
        void ConstructWithValidNvm(services::CalibrationData data = MakeDefaultCalibrationData());

        void SetupCalibrationExpectations();

        // Must be called after ConstructWithInvalidNvm() or ConstructWithValidNvm().
        void SetupCanIntegration();

        void InjectCanStart();
        void InjectCanStop();
        void InjectCanClearFault();
        void InjectCanEmergencyStop();

        void DeferClearCalibration();
        void CompleteInvalidate(services::NvmStatus status);

        void CompletePolePairsEstimation(std::size_t polePairs);
        void CompleteRLEstimation(foc::Ohm resistance, foc::MilliHenry inductance);
        void CompleteAlignment(foc::Radians offset);

        static services::CalibrationData MakeDefaultCalibrationData();

        static const foc::Volts testVdc;

        using TorqueStateMachine = application::TorqueStateMachine;

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

        testing::StrictMock<PlatformFactoryMock> platformFactory;
        EepromStub eepromStub;

        services::NvmEepromRegion calibrationRegion{ eepromStub, 0, 128 };
        services::NvmEepromRegion configRegion{ eepromStub, 128, 128 };
        services::NonVolatileMemoryImpl nvm{ calibrationRegion, configRegion };

        testing::StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        testing::StrictMock<services::MotorAlignmentMock> alignmentMock;
        testing::StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;
        // A fault, an emergency stop and the destructor each release the calibration services and
        // the fault registration; the scenarios that assert on those set their own expectations.
        infra::Execute setupTeardownExpectations{ [this]()
            {
                using namespace testing;
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
            } };

        std::optional<TorqueStateMachine> motorStateMachine;

        bool calibrationExpectationsConfigured{ false };
        infra::Function<void(std::optional<std::size_t>)> capturedPolePairsCallback;
        infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> capturedRLCallback;
        infra::Function<void(std::optional<foc::Radians>)> capturedAlignmentCallback;

        testing::StrictMock<hal::CanMock> transportCanMock;
        std::optional<services::CanProtocolServer> canProtocolServer;
        std::optional<can::FocMotorCategoryServer> motorCategoryServer;
        std::optional<FocMotorStateMachineBridge> motorBridge;
    };
}
