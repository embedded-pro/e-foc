#include "core/platform_abstraction/test_doubles/CanBusAdapterMock.hpp"
#include "core/platform_abstraction/test_doubles/PlatformFactoryMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/PlatformFaultNotifier.hpp"
#include "core/state_machine/TorqueStateMachine.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "services/tracer/Tracer.hpp"
#include <gmock/gmock.h>

namespace
{
    using namespace testing;

    class TestPlatformFaultNotifier
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        StrictMock<hal::SerialCommunicationMock> communication;
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

        StrictMock<application::PlatformFactoryMock> platformFactory;
        StrictMock<application::CanBusAdapterMock> canBusMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;

        foc::Volts vdc{ 24.0f };

        infra::Execute setupPlatformExpectations{ [this]()
            {
                EXPECT_CALL(platformFactory, RegisterBoardProtection(_))
                    .WillOnce(Invoke([this](const infra::Function<void(application::PlatformFactory::BoardProtectionReason)>& handler)
                        {
                            platformFactory.StoreBoardProtectionHandler(handler);
                        }));
                EXPECT_CALL(platformFactory, CanBus()).Times(AnyNumber()).WillRepeatedly(ReturnRef(canBusMock));
                EXPECT_CALL(canBusMock, SetOnError(_)).Times(AnyNumber());
                EXPECT_CALL(platformFactory, BaseFrequency()).Times(AnyNumber()).WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(platformFactory, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(platformFactory, Stop()).Times(AnyNumber());
                EXPECT_CALL(platformFactory, Read()).Times(AnyNumber()).WillRepeatedly(Return(foc::Radians{ 0.0f }));
                EXPECT_CALL(platformFactory, Set(_)).Times(AnyNumber());
                EXPECT_CALL(platformFactory, SetZero()).Times(AnyNumber());
            } };

        state_machine::PlatformFaultNotifier faultNotifier{ platformFactory };

        void GivenCalibrationInNvm()
        {
            services::CalibrationData data{};
            data.polePairs = 7;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.lQ = 1.0f;

            EXPECT_CALL(nvmMock, IsCalibrationValid(_))
                .WillOnce(Invoke([](infra::Function<void(bool)> onDone)
                    {
                        onDone(true);
                    }));
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([data](services::CalibrationData& out, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = data;
                        onDone(services::NvmStatus::Ok);
                    }));
        }

        application::TorqueStateMachine CreateStateMachine()
        {
            return application::TorqueStateMachine{
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ platformFactory, platformFactory, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock },
                faultNotifier,
                state_machine::TransitionPolicy::Cli
            };
        }
    };
}

TEST_F(TestPlatformFaultNotifier, board_protection_from_enabled_stops_inverter_and_enters_fault)
{
    GivenCalibrationInNvm();
    auto sm = CreateStateMachine();

    EXPECT_CALL(platformFactory, Start()).Times(1);
    sm.CmdEnable();

    EXPECT_CALL(platformFactory, Stop()).Times(AtLeast(1));
    platformFactory.RaiseBoardProtection(application::PlatformFactory::BoardProtectionReason::overCurrent);

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code, state_machine::FaultCode::overcurrent);
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(TestPlatformFaultNotifier, over_voltage_maps_to_overvoltage_fault_code)
{
    GivenCalibrationInNvm();
    auto sm = CreateStateMachine();

    platformFactory.RaiseBoardProtection(application::PlatformFactory::BoardProtectionReason::overVoltage);

    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overvoltage);
}

TEST_F(TestPlatformFaultNotifier, over_temperature_maps_to_overtemperature_fault_code)
{
    GivenCalibrationInNvm();
    auto sm = CreateStateMachine();

    platformFactory.RaiseBoardProtection(application::PlatformFactory::BoardProtectionReason::overTemperature);

    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overtemperature);
}

TEST_F(TestPlatformFaultNotifier, last_fault_code_is_none_before_any_fault)
{
    GivenCalibrationInNvm();
    auto sm = CreateStateMachine();

    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::none);
}

TEST_F(TestPlatformFaultNotifier, board_protection_before_registration_is_discarded)
{
    platformFactory.RaiseBoardProtection(application::PlatformFactory::BoardProtectionReason::overCurrent);
}
