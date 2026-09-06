#include "can-lite/core/test/CanMock.hpp"
#include "can-lite/server/CanProtocolServer.hpp"
#include "core/can/CanLivenessWatchdog.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <gtest/gtest.h>
#include <optional>

namespace
{
    using namespace testing;

    class CanLivenessWatchdogTest
        : public ::testing::Test
        , public infra::ClockFixture
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

        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<services::MechanicalParametersIdentificationMock> mechIdentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;
        // A fault, an emergency stop and the destructor each release the calibration services and
        // the fault registration; the tests that assert on those set their own expectations.
        infra::Execute setupTeardownExpectations{ [this]()
            {
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
            } };
        StrictMock<hal::CanMock> canMock;

        infra::Execute setupHardwareExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency()).Times(AnyNumber()).WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Start()).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Unregister()).Times(AnyNumber());
                EXPECT_CALL(canMock, SendData(_, _, _)).Times(AnyNumber());
                EXPECT_CALL(canMock, ReceiveData(_)).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Register(_)).Times(AnyNumber());
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
                            done(services::NvmStatus::Ok);
                        }));
            } };

        std::optional<services::CanProtocolServer> server;
        std::optional<state_machine::ControlModeStateMachine> controlMode;
        std::optional<can::CanLivenessWatchdog> watchdog;

        void Construct()
        {
            services::ConfigData config{};
            config.defaultControlMode = 0;

            server.emplace(canMock, services::CanProtocolServer::Config{ .nodeId = 1 });
            controlMode.emplace(
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
            watchdog.emplace(*server, *controlMode, tracer);
            ExecuteAllActions();
        }

        bool IsEnabled() const
        {
            return std::holds_alternative<state_machine::Enabled>(controlMode->ActiveStateMachine().CurrentState());
        }
    };
}

TEST_F(CanLivenessWatchdogTest, LosingTheClientWhileStoppedLeavesTheStateMachineAlone)
{
    Construct();

    const auto before = controlMode->ActiveStateMachine().CurrentState().index();

    watchdog->Offline();

    EXPECT_EQ(controlMode->ActiveStateMachine().CurrentState().index(), before);
}

TEST_F(CanLivenessWatchdogTest, LosingTheClientWhileEnabledStopsTheDrive)
{
    Construct();

    ASSERT_EQ(controlMode->ActiveStateMachine().CmdEnable(), state_machine::CommandResult::ok);
    ASSERT_TRUE(IsEnabled());

    watchdog->Offline();

    EXPECT_FALSE(IsEnabled());
}

TEST_F(CanLivenessWatchdogTest, RegainingTheClientDoesNotDisturbTheStateMachine)
{
    Construct();

    ASSERT_EQ(controlMode->ActiveStateMachine().CmdEnable(), state_machine::CommandResult::ok);

    watchdog->Online();

    EXPECT_TRUE(IsEnabled());
}
