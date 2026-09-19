#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/platform_abstraction/test_doubles/PlatformFactoryMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "core/supervision/WatchdogSupervisor.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/util/Terminal.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>

namespace
{
    using namespace testing;

    class WatchdogSupervisorTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static constexpr std::chrono::microseconds deadline{ std::chrono::milliseconds(100) };
        static constexpr std::chrono::microseconds startupGrace{ std::chrono::milliseconds(500) };
        static constexpr uint32_t evaluationsPerDeadline{ 4 };
        static constexpr std::chrono::milliseconds evaluationPeriod{ 25 };

        uint32_t powerStageStops{ 0 };

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
        services::TerminalWithStorage::WithMaxSize<22> terminal{ terminalWithCommands, tracer };

        StrictMock<application::PlatformFactoryMock> hardware;
        StrictMock<drivers::WatchdogMock> watchdogMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<services::MechanicalParametersIdentificationMock> mechIdentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;

        infra::Execute setupHardwareExpectations{ [this]()
            {
                EXPECT_CALL(hardware, Watchdog()).Times(AnyNumber()).WillRepeatedly(ReturnRef(watchdogMock));
                EXPECT_CALL(watchdogMock, Enable(_, _)).Times(AnyNumber()).WillRepeatedly(Invoke(&watchdogMock, &drivers::WatchdogMock::StoreDeadlineMissedHandler));
                EXPECT_CALL(hardware, BaseFrequency()).Times(AnyNumber()).WillRepeatedly(Return(hal::Hertz{ 20000 }));
                EXPECT_CALL(hardware, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(hardware, Start()).Times(AnyNumber());
                EXPECT_CALL(hardware, Stop()).Times(AnyNumber()).WillRepeatedly(Invoke([this]()
                    {
                        ++powerStageStops;
                    }));
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Unregister()).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Register(_, _)).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, Abort()).Times(AnyNumber());
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
                            data.inertia = 0.001f;
                            data.frictionViscous = 0.0001f;
                            data.speedLoopBandwidth = 50.0f;
                            data.stage = services::CalibrationStage::complete;
                            done(services::NvmStatus::Ok);
                        }));
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .Times(AnyNumber())
                    .WillRepeatedly(Invoke([](const services::CalibrationData&,
                                               const infra::Function<void(services::NvmStatus)>& done)
                        {
                            done(services::NvmStatus::Ok);
                        }));
            } };

        supervision::ControlHealth health;
        supervision::WatchdogSupervisor supervisor{ hardware, health, tracer };
        std::optional<state_machine::ControlModeStateMachine> controlMode;

        void Enable()
        {
            supervisor.Enable({ deadline, startupGrace, evaluationsPerDeadline });
        }

        void AttachControlMode(state_machine::ControlMode mode)
        {
            services::ConfigData config{};
            config.defaultControlMode = static_cast<uint8_t>(mode);

            controlMode.emplace(
                application::TerminalAndTracer{ terminal, tracer },
                application::MotorHardware{ hardware, encoderMock, foc::Volts{ 24.0f } },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock) },
                faultNotifierMock,
                config,
                state_machine::ControlModeStateMachine::OuterLoopArgs{
                    foc::Ampere{ 10.0f },
                    hal::Hertz{ 1000 },
                    lowPriorityInterruptMock });
            ExecuteAllActions();
            supervisor.AttachControlMode(*controlMode);
        }

        void EnterEnabled()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                .WillOnce(Invoke([](std::size_t, const auto&,
                                     const infra::Function<void(std::optional<foc::Radians>)>& cb)
                    {
                        cb(foc::Radians{ 0.0f });
                    }));
            controlMode->CmdReAlign([](state_machine::CommandResult) {});
            ASSERT_EQ(state_machine::CommandResult::ok, controlMode->ActiveStateMachine().CmdEnable());
        }

        void RunOneEvaluationWithBothLoopsProgressing()
        {
            health.InnerLoopProgress().Signal();
            health.OuterLoopProgress().Signal();
            ForwardTime(evaluationPeriod);
        }
    };
}

TEST_F(WatchdogSupervisorTest, enabling_supervision_arms_the_port_with_the_configured_deadline)
{
    EXPECT_CALL(watchdogMock, Enable(deadline, _)).WillOnce(Invoke(&watchdogMock, &drivers::WatchdogMock::StoreDeadlineMissedHandler));

    Enable();
}

TEST_F(WatchdogSupervisorTest, supervision_state_is_read_back_from_the_port)
{
    EXPECT_CALL(watchdogMock, IsEnabled()).WillOnce(Return(true));

    EXPECT_TRUE(supervisor.IsSupervising());
}

TEST_F(WatchdogSupervisorTest, the_startup_grace_keeps_feeding_before_a_control_mode_is_attached)
{
    Enable();

    EXPECT_CALL(watchdogMock, Feed()).Times(static_cast<int>(startupGrace / evaluationPeriod));

    ForwardTime(std::chrono::duration_cast<std::chrono::milliseconds>(startupGrace));
}

TEST_F(WatchdogSupervisorTest, a_zero_startup_grace_withholds_the_feed_from_the_first_evaluation)
{
    supervisor.Enable({ deadline, std::chrono::microseconds::zero(), evaluationsPerDeadline });

    ForwardTime(std::chrono::milliseconds(1000));
}

TEST_F(WatchdogSupervisorTest, a_boot_that_never_attaches_a_control_mode_stops_being_fed)
{
    Enable();

    EXPECT_CALL(watchdogMock, Feed()).Times(AnyNumber());
    ForwardTime(std::chrono::duration_cast<std::chrono::milliseconds>(startupGrace));
    Mock::VerifyAndClearExpectations(&watchdogMock);

    ForwardTime(std::chrono::milliseconds(1000));
}

TEST_F(WatchdogSupervisorTest, a_stopped_drive_is_fed_without_any_control_loop_progress)
{
    Enable();
    AttachControlMode(state_machine::ControlMode::torque);

    EXPECT_CALL(watchdogMock, Feed()).Times(4);

    ForwardTime(evaluationPeriod * 4);
}

TEST_F(WatchdogSupervisorTest, an_enabled_speed_drive_is_fed_while_both_loops_progress)
{
    Enable();
    AttachControlMode(state_machine::ControlMode::speed);
    EnterEnabled();

    EXPECT_CALL(watchdogMock, Feed()).Times(3);

    RunOneEvaluationWithBothLoopsProgressing();
    RunOneEvaluationWithBothLoopsProgressing();
    RunOneEvaluationWithBothLoopsProgressing();
}

TEST_F(WatchdogSupervisorTest, an_enabled_speed_drive_stops_being_fed_when_the_outer_loop_stalls)
{
    Enable();
    AttachControlMode(state_machine::ControlMode::speed);
    EnterEnabled();

    EXPECT_CALL(watchdogMock, Feed()).Times(1);

    RunOneEvaluationWithBothLoopsProgressing();

    health.InnerLoopProgress().Signal();
    ForwardTime(evaluationPeriod);
    health.InnerLoopProgress().Signal();
    ForwardTime(evaluationPeriod);
}

TEST_F(WatchdogSupervisorTest, an_enabled_torque_drive_is_fed_without_outer_loop_progress)
{
    Enable();
    AttachControlMode(state_machine::ControlMode::torque);
    EnterEnabled();

    EXPECT_CALL(watchdogMock, Feed()).Times(2);

    health.InnerLoopProgress().Signal();
    ForwardTime(evaluationPeriod);
    health.InnerLoopProgress().Signal();
    ForwardTime(evaluationPeriod);
}

TEST_F(WatchdogSupervisorTest, a_missed_deadline_stops_the_power_stage_before_it_resets)
{
    Enable();
    AttachControlMode(state_machine::ControlMode::torque);
    EnterEnabled();

    const auto stopsBeforeMiss = powerStageStops;
    uint32_t stopsSeenAtReset{ 0 };
    EXPECT_CALL(hardware, ResetFromWatchdogExpiry()).WillOnce(Invoke([&]()
        {
            stopsSeenAtReset = powerStageStops;
        }));

    watchdogMock.RaiseDeadlineMissed();

    EXPECT_GT(stopsSeenAtReset, stopsBeforeMiss);
    EXPECT_FALSE(std::holds_alternative<state_machine::Enabled>(controlMode->ActiveStateMachine().CurrentState()));
}

TEST_F(WatchdogSupervisorTest, a_missed_deadline_during_startup_resets_without_a_control_mode)
{
    Enable();

    EXPECT_CALL(hardware, ResetFromWatchdogExpiry());

    watchdogMock.RaiseDeadlineMissed();
}

TEST_F(WatchdogSupervisorTest, evaluation_stops_after_a_missed_deadline)
{
    Enable();

    EXPECT_CALL(hardware, ResetFromWatchdogExpiry());
    watchdogMock.RaiseDeadlineMissed();

    ForwardTime(std::chrono::milliseconds(1000));
}
