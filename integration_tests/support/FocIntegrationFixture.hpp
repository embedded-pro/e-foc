#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/foc/implementations/FocTorqueImpl.hpp"
#include "core/platform_abstraction/MotorFieldOrientedControllerAdapter.hpp"
#include "core/platform_abstraction/test_doubles/AdcPhaseCurrentMeasurementMock.hpp"
#include "core/platform_abstraction/test_doubles/QuadratureEncoderDecoratorMock.hpp"
#include "core/platform_abstraction/test_doubles/SynchronousThreeChannelsPwmMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/cli/TerminalTorque.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemoryImpl.hpp"
#include "core/services/non_volatile_memory/NvmEepromRegion.hpp"
#include "core/state_machine/FocStateMachineImpl.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/OutputStream.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/test_helper/ProxyCreatorMock.hpp"
#include "integration_tests/support/EepromStub.hpp"
#include "integration_tests/support/FocMotorStateMachineBridge.hpp"
#include "integration_tests/support/PlatformFactoryMock.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <gmock/gmock.h>
#include <optional>

namespace integration
{
    class StreamWriterMock
        : public infra::StreamWriter
    {
    public:
        using StreamWriter::StreamWriter;

        MOCK_METHOD(void, Insert, (infra::ConstByteRange range, infra::StreamErrorPolicy& errorPolicy), (override));
        MOCK_METHOD(std::size_t, Available, (), (const, override));
        MOCK_METHOD(std::size_t, ConstructSaveMarker, (), (const, override));
        MOCK_METHOD(std::size_t, GetProcessedBytesSince, (std::size_t marker), (const, override));
        MOCK_METHOD(infra::ByteRange, SaveState, (std::size_t marker), (override));
        MOCK_METHOD(void, RestoreState, (infra::ByteRange range), (override));
        MOCK_METHOD(infra::ByteRange, Overwrite, (std::size_t marker), (override));
    };

    struct FocIntegrationFixture
        : infra::EventDispatcherWithWeakPtrFixture
    {
        FocIntegrationFixture();

        // Helpers for deferred state machine construction
        void ConstructWithInvalidNvm();
        void ConstructWithValidNvm(services::CalibrationData data = MakeDefaultCalibrationData());

        // Set up expectations on service mocks so the full calibration sequence can proceed.
        void SetupCalibrationExpectations();

        // Wire up the CAN category server and bridge to the state machine.
        // Must be called after ConstructWithInvalidNvm() or ConstructWithValidNvm().
        void SetupCanIntegration();

        // Inject a CAN message directly into the category server.
        void InjectCanStart();
        void InjectCanStop();
        void InjectCanClearFault();

        // Complete a previously captured calibration step callback.
        void CompletePolePairsEstimation(std::size_t polePairs);
        void CompleteRLEstimation(foc::Ohm resistance, foc::MilliHenry inductance);
        void CompleteAlignment(foc::Radians offset);

        static services::CalibrationData MakeDefaultCalibrationData();

        static const foc::Volts testVdc;

        using TorqueStateMachine = application::FocStateMachineImpl<
            foc::FocTorqueImpl,
            services::TerminalFocTorqueInteractor,
            state_machine::AutoTransitionPolicy>;

        testing::StrictMock<StreamWriterMock> streamWriterMock;
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

        testing::StrictMock<application::AdcPhaseCurrentMeasurementMock> adcPhaseCurrentMock;
        testing::StrictMock<hal::SynchronousThreeChannelsPwmMock> pwmMock;
        testing::StrictMock<application::QuadratureEncoderDecoratorMock> encoderDecoratorMock;

        infra::CreatorMock<application::AdcPhaseCurrentMeasurement,
            void(application::PlatformFactory::SampleAndHold)> adcCreator{ adcPhaseCurrentMock };
        infra::CreatorMock<hal::SynchronousThreeChannelsPwm,
            void(std::chrono::nanoseconds, hal::Hertz)> pwmCreator{ pwmMock };
        infra::CreatorMock<application::QuadratureEncoderDecorator, void()> encoderCreator{ encoderDecoratorMock };

        testing::StrictMock<PlatformFactoryMock> platformFactory;
        EepromStub eepromStub;

        std::optional<application::PlatformAdapter> platformAdapter;

        services::NvmEepromRegion calibrationRegion{ eepromStub, 0, 128 };
        services::NvmEepromRegion configRegion{ eepromStub, 128, 128 };
        services::NonVolatileMemoryImpl nvm{ calibrationRegion, configRegion };

        testing::StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        testing::StrictMock<services::MotorAlignmentMock> alignmentMock;
        testing::StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;

        std::optional<TorqueStateMachine> motorStateMachine;

        bool calibrationExpectationsConfigured{ false };
        infra::Function<void(std::optional<std::size_t>)> capturedPolePairsCallback;
        infra::Function<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)> capturedRLCallback;
        infra::Function<void(std::optional<foc::Radians>)> capturedAlignmentCallback;

        testing::StrictMock<hal::CanMock> transportCanMock;
        std::optional<services::CanFrameTransport> canTransport;
        std::optional<services::FocMotorCategoryServer> motorCategoryServer;
        std::optional<FocMotorStateMachineBridge> motorBridge;
    };
}
