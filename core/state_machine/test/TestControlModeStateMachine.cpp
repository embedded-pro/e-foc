#include "core/foc/cascade/PositionCascade.hpp"
#include "core/foc/cascade/SpeedCascade.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/interfaces/test_doubles/ExecutionMock.hpp"
#include "core/platform_abstraction/interfaces/test_doubles/DriversMock.hpp"
#include "core/services/alignment/test_doubles/MotorAlignmentMock.hpp"
#include "core/services/electrical_system_ident/test_doubles/ElectricalParametersIdentificationMock.hpp"
#include "core/services/mechanical_system_ident/test_doubles/MechanicalParametersIdentificationMock.hpp"
#include "core/services/non_volatile_memory/test_doubles/NonVolatileMemoryMock.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "core/state_machine/test_doubles/FaultNotifierMock.hpp"
#include "infra/stream/test/StreamMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/tracer/Tracer.hpp"
#include <bit>
#include <cstdint>
#include <gtest/gtest.h>
#include <utility>

using namespace testing;

namespace
{
    float NonFinite(uint32_t bits)
    {
        volatile uint32_t opaque = bits;

        return std::bit_cast<float>(static_cast<uint32_t>(opaque));
    }

    float Infinity()
    {
        return NonFinite(0x7F800000u);
    }

    float NotANumber()
    {
        return NonFinite(0x7FC00000u);
    }
}

using TestedControlMode = state_machine::ControlModeStateMachine;

namespace
{
    class ControlModeStateMachineTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        StrictMock<infra::StreamWriterMock> streamWriterMock;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriterMock };
        services::TracerToStream tracer{ stream };
        infra::Execute setupStreamExpectations{ [this]()
            {
                EXPECT_CALL(streamWriterMock, Insert(_, _)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Available()).Times(AnyNumber()).WillRepeatedly(Return(1000));
                EXPECT_CALL(streamWriterMock, ConstructSaveMarker()).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, GetProcessedBytesSince(_)).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriterMock, SaveState(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(streamWriterMock, RestoreState(_)).Times(AnyNumber());
                EXPECT_CALL(streamWriterMock, Overwrite(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
            } };

        StrictMock<drivers::ThreePhaseInverterMock> inverterMock;
        StrictMock<drivers::EncoderMock> encoderMock;
        StrictMock<foc::LowPriorityInterruptMock> lowPriorityInterruptMock;
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<services::MechanicalParametersIdentificationMock> mechIdentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;
        infra::Execute setupTeardownExpectations{ [this]()
            {
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(mechIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(electricalIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(mechIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
            } };

        infra::Execute setupHardwareExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Register(_)).Times(AnyNumber());
                EXPECT_CALL(lowPriorityInterruptMock, Unregister()).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Register(_, _))
                    .Times(AnyNumber())
                    .WillRepeatedly(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& immediate, const infra::Function<void(state_machine::FaultCode)>& deferred)
                        {
                            faultNotifierMock.StoreHandler(immediate, deferred);
                        }));
            } };

        std::optional<TestedControlMode> subject;

        void GivenNvmAlwaysInvalid()
        {
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillRepeatedly(Invoke([](services::CalibrationData& out, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = services::CalibrationData{};
                        onDone(services::NvmStatus::InvalidData);
                    }));
        }

        void GivenNvmSaveConfigSucceeds()
        {
            EXPECT_CALL(nvmMock, SaveConfig(_, _))
                .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        onDone(services::NvmStatus::Ok);
                    }));
        }

        void GivenNvmSaveConfigAlwaysSucceeds()
        {
            EXPECT_CALL(nvmMock, SaveConfig(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        onDone(services::NvmStatus::Ok);
                    }));
        }

        void GivenNvmSaveConfigFails()
        {
            EXPECT_CALL(nvmMock, SaveConfig(_, _))
                .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        onDone(services::NvmStatus::WriteFailed);
                    }));
        }

        void ConstructSubject(uint8_t defaultMode = 0)
        {
            services::ConfigData config{};
            config.defaultControlMode = defaultMode;
            ConstructSubjectWithConfig(config);
        }

        void ConstructSubjectWithConfig(const services::ConfigData& config)
        {
            subject.emplace(
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), foc::Weber{ 0.007f } },
                faultNotifierMock,
                config,
                TestedControlMode::OuterLoopArgs{
                    foc::Ampere{ 10.0f },
                    hal::Hertz{ 1000 },
                    lowPriorityInterruptMock });
        }
    };
}

TEST_F(ControlModeStateMachineTest, Active_Returns_Torque_After_Default_Construction)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject(/*defaultMode=*/0);

    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::torque);
}

TEST_F(ControlModeStateMachineTest, Active_Returns_Speed_When_Constructed_With_Speed_Default)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject(/*defaultMode=*/static_cast<uint8_t>(state_machine::ControlMode::speed));

    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::speed);
}

TEST_F(ControlModeStateMachineTest, TrySetSpeed_Returns_False_When_Torque_Active)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_FALSE(subject->Commands().TrySetSpeed(foc::RadiansPerSecond{ 10.0f }));
}

TEST_F(ControlModeStateMachineTest, TrySetPosition_Returns_False_When_Torque_Active)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_FALSE(subject->Commands().TrySetPosition(foc::Radians{ 1.0f }));
}

TEST_F(ControlModeStateMachineTest, TrySetTorque_Returns_False_When_Speed_Active)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    EXPECT_FALSE(subject->Commands().TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } }));
}

TEST_F(ControlModeStateMachineTest, Select_Switches_Active_To_Speed_On_Nvm_Ok)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    state_machine::SelectResult result{ state_machine::SelectResult::nvmFailed };
    subject->Modes().Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::ok);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::speed);
}

TEST_F(ControlModeStateMachineTest, Select_Switches_Active_To_Position_On_Nvm_Ok)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    state_machine::SelectResult result{ state_machine::SelectResult::nvmFailed };
    subject->Modes().Select(state_machine::ControlMode::position, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::ok);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::position);
}

TEST_F(ControlModeStateMachineTest, Select_RollsBack_Mode_On_Nvm_Failure)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigFails();
    ConstructSubject();

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::nvmFailed);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::torque);
}

TEST_F(ControlModeStateMachineTest, Select_Remains_Usable_After_Nvm_Reports_Busy)
{
    GivenNvmAlwaysInvalid();

    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Busy);
            }))
        .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));

    ConstructSubject();

    state_machine::SelectResult busyResult{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::speed, [&busyResult](state_machine::SelectResult r)
        {
            busyResult = r;
        });

    EXPECT_EQ(busyResult, state_machine::SelectResult::busy);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::torque);

    state_machine::SelectResult retryResult{ state_machine::SelectResult::nvmFailed };
    subject->Modes().Select(state_machine::ControlMode::speed, [&retryResult](state_machine::SelectResult r)
        {
            retryResult = r;
        });

    EXPECT_EQ(retryResult, state_machine::SelectResult::ok);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::speed);
}

TEST_F(ControlModeStateMachineTest, OnSaveConfigDone_DoesNotActivate_When_Fault_Latched_During_Nvm_Write)
{
    GivenNvmAlwaysInvalid();

    infra::Function<void(services::NvmStatus)> deferredSave;
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&deferredSave](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                deferredSave = onDone;
            }))
        .WillOnce(Invoke([](const services::ConfigData& config, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_EQ(config.defaultControlMode, static_cast<uint8_t>(state_machine::ControlMode::torque));
                onDone(services::NvmStatus::Ok);
            }));

    ConstructSubject();

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);

    deferredSave(services::NvmStatus::Ok);

    EXPECT_EQ(result, state_machine::SelectResult::busy);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::torque);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(
        subject->Drive().ActiveStateMachine().CurrentState()));
    EXPECT_EQ(subject->Drive().ActiveStateMachine().LastFaultCode(), state_machine::FaultCode::hardwareFault);

    state_machine::SelectResult retryResult{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::speed, [&retryResult](state_machine::SelectResult r)
        {
            retryResult = r;
        });

    EXPECT_EQ(retryResult, state_machine::SelectResult::busy);
}

TEST_F(ControlModeStateMachineTest, Calibrate_Is_Refused_While_Select_Nvm_Write_Is_In_Flight)
{
    GivenNvmAlwaysInvalid();

    infra::Function<void(services::NvmStatus)> deferredSave;
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&deferredSave](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                deferredSave = onDone;
            }));

    ConstructSubject();

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    state_machine::CommandResult calibrateResult{ state_machine::CommandResult::ok };
    subject->Drive().ActiveStateMachine().CmdCalibrate([&calibrateResult](state_machine::CommandResult r)
        {
            calibrateResult = r;
        });

    EXPECT_EQ(calibrateResult, state_machine::CommandResult::rejected);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(
        subject->Drive().ActiveStateMachine().CurrentState()));

    deferredSave(services::NvmStatus::Ok);

    EXPECT_EQ(result, state_machine::SelectResult::ok);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::speed);
}

TEST_F(ControlModeStateMachineTest, Select_While_Previous_Select_Pending_Reports_Busy)
{
    GivenNvmAlwaysInvalid();

    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([](const services::ConfigData&, infra::Function<void(services::NvmStatus)>) {}));

    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::speed, [](state_machine::SelectResult) {});

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::position, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::busy);
}

TEST_F(ControlModeStateMachineTest, Select_While_Active_Machine_Has_Pending_Nvm_Command_Reports_Busy)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    infra::Function<void(services::NvmStatus)> deferredInvalidate;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&deferredInvalidate](infra::Function<void(services::NvmStatus)> onDone)
            {
                deferredInvalidate = onDone;
            }));

    subject->Drive().ActiveStateMachine().CmdClearCalibration([](state_machine::CommandResult) {});

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::busy);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::torque);

    deferredInvalidate(services::NvmStatus::Ok);

    GivenNvmSaveConfigSucceeds();
    state_machine::SelectResult retryResult{ state_machine::SelectResult::nvmFailed };
    subject->Modes().Select(state_machine::ControlMode::speed, [&retryResult](state_machine::SelectResult r)
        {
            retryResult = r;
        });

    EXPECT_EQ(retryResult, state_machine::SelectResult::ok);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::speed);
}

TEST_F(ControlModeStateMachineTest, Select_While_Boot_Nvm_Load_In_Flight_Reports_Busy)
{
    infra::Function<void(services::NvmStatus)> deferredLoad;
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([&deferredLoad](services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                deferredLoad = onDone;
            }))
        .WillRepeatedly(Invoke([](services::CalibrationData& out, infra::Function<void(services::NvmStatus)> onDone)
            {
                out = services::CalibrationData{};
                onDone(services::NvmStatus::InvalidData);
            }));

    ConstructSubject();

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::busy);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::torque);

    deferredLoad(services::NvmStatus::InvalidData);

    GivenNvmSaveConfigSucceeds();
    state_machine::SelectResult retryResult{ state_machine::SelectResult::nvmFailed };
    subject->Modes().Select(state_machine::ControlMode::speed, [&retryResult](state_machine::SelectResult r)
        {
            retryResult = r;
        });

    EXPECT_EQ(retryResult, state_machine::SelectResult::ok);
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::speed);
}

namespace
{
    class ControlModeStateMachineExtTest
        : public ControlModeStateMachineTest
    {
    public:
        void GivenNvmAlwaysValid()
        {
            services::CalibrationData data{};
            data.polePairs = 7;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.stage = services::CalibrationStage::complete;
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([data](services::CalibrationData& out,
                                           infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = data;
                        onDone(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
        }

        void GivenNvmAlwaysValidWithMechanics()
        {
            services::CalibrationData data{};
            data.polePairs = 7;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.inertia = 1.0e-5f;
            data.frictionViscous = 1.0e-4f;
            data.stage = services::CalibrationStage::complete;
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .Times(AnyNumber())
                .WillRepeatedly(Invoke([data](services::CalibrationData& out,
                                           infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = data;
                        onDone(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
        }

        bool IsReadyWithRotorReference() const
        {
            const auto* ready = std::get_if<state_machine::Ready>(&subject->Drive().ActiveStateMachine().CurrentState());
            return ready != nullptr && ready->rotorReferenceValid;
        }

        void GivenNvmValid()
        {
            services::CalibrationData data{};
            data.polePairs = 7;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.stage = services::CalibrationStage::complete;
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([data](services::CalibrationData& out,
                                     infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = data;
                        onDone(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
        }

        void AlignAfterBoot()
        {
            EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                .WillOnce(Invoke([](std::size_t, const auto&,
                                     const infra::Function<void(std::optional<foc::Radians>)>& cb)
                    {
                        cb(foc::Radians{ 0.0f });
                    }));
            EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                .WillOnce(Invoke([](const services::CalibrationData&,
                                     infra::Function<void(services::NvmStatus)> onDone)
                    {
                        onDone(services::NvmStatus::Ok);
                    }));
            subject->Drive().ActiveCommon().Commissioning().CmdReAlign([](state_machine::CommandResult) {});
            ExecuteAllActions();
        }

        void ConstructSubjectWithFluxLinkage(foc::Weber fluxLinkage)
        {
            services::ConfigData config{};
            subject.emplace(
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, foc::Volts{ 24.0f } },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock, std::ref(mechIdentMock), fluxLinkage },
                faultNotifierMock,
                config,
                TestedControlMode::OuterLoopArgs{
                    foc::Ampere{ 10.0f },
                    hal::Hertz{ 1000 },
                    lowPriorityInterruptMock });
        }
    };
}

TEST_F(ControlModeStateMachineExtTest, DecoupledPidIsSelectableWithConfiguredFluxLinkage)
{
    GivenNvmValid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubjectWithFluxLinkage(foc::Weber{ 0.007f });

    EXPECT_EQ(subject->Algorithms().SelectCurrentAlgorithm(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::ok);
}

TEST_F(ControlModeStateMachineExtTest, DecoupledPidIsRefusedWithoutFluxLinkage)
{
    GivenNvmValid();
    ConstructSubjectWithFluxLinkage(foc::Weber{ 0.0f });

    EXPECT_EQ(subject->Algorithms().SelectCurrentAlgorithm(foc::CurrentAlgorithm::decoupledPid), foc::SelectResult::invalidParameters);
}

TEST_F(ControlModeStateMachineExtTest, PersistedFluxLinkageOverridesTheConfiguredDefault)
{
    services::CalibrationData stored{};
    stored.polePairs = 7;
    stored.rPhase = 0.5f;
    stored.lD = 1.0f;
    stored.fluxLinkage = 0.02f;
    stored.stage = services::CalibrationStage::complete;

    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([stored](services::CalibrationData& out, infra::Function<void(services::NvmStatus)> onDone)
            {
                out = stored;
                onDone(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    ConstructSubjectWithFluxLinkage(foc::Weber{ 0.007f });

    EXPECT_NEAR(subject->Drive().ActiveCommon().Calibration().ActiveFluxLinkage().Value(), 0.02f, 1e-6f);
}

TEST_F(ControlModeStateMachineExtTest, Active_Returns_Position_When_Constructed_With_Position_Default)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject(/*defaultMode=*/static_cast<uint8_t>(state_machine::ControlMode::position));

    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::position);
}

TEST_F(ControlModeStateMachineExtTest, TrySetTorque_Returns_True_When_Torque_Mode_Active)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_TRUE(subject->Commands().TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } }));
}

TEST_F(ControlModeStateMachineExtTest, TrySetSpeed_Returns_True_When_Speed_Mode_Active)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    EXPECT_TRUE(subject->Commands().TrySetSpeed(foc::RadiansPerSecond{ 10.0f }));
}

TEST_F(ControlModeStateMachineExtTest, a_speed_setpoint_beyond_the_overspeed_limit_is_held_at_the_limit)
{
    GivenNvmAlwaysInvalid();
    services::ConfigData config{};
    config.defaultControlMode = static_cast<uint8_t>(state_machine::ControlMode::speed);
    config.limits.overspeedLimit = 50.0f;
    ConstructSubjectWithConfig(config);

    EXPECT_TRUE(subject->Commands().TrySetSpeed(foc::RadiansPerSecond{ 80.0f }));
    EXPECT_NEAR(subject->Drive().ActiveStateMachine().ObserveMotion().demandedSpeed.Value(), 50.0f, 1e-6f);

    EXPECT_TRUE(subject->Commands().TrySetSpeed(foc::RadiansPerSecond{ -80.0f }));
    EXPECT_NEAR(subject->Drive().ActiveStateMachine().ObserveMotion().demandedSpeed.Value(), -50.0f, 1e-6f);

    EXPECT_TRUE(subject->Commands().TrySetSpeed(foc::RadiansPerSecond{ 20.0f }));
    EXPECT_NEAR(subject->Drive().ActiveStateMachine().ObserveMotion().demandedSpeed.Value(), 20.0f, 1e-6f);
}

TEST_F(ControlModeStateMachineExtTest, a_resetStoredoverspeed_limit_outside_its_range_is_replaced_by_its_default)
{
    GivenNvmAlwaysInvalid();
    services::ConfigData config{};
    config.limits.overspeedLimit = -3.0f;
    ConstructSubjectWithConfig(config);

    EXPECT_NEAR(subject->Config().limits.overspeedLimit, services::LimitSettings{}.overspeedLimit, 1e-6f);
}

TEST_F(ControlModeStateMachineExtTest, resetStoredtravel_limits_hold_the_position_reference)
{
    GivenNvmAlwaysInvalid();
    services::ConfigData config{};
    config.defaultControlMode = static_cast<uint8_t>(state_machine::ControlMode::position);
    config.limits.positionMinimum = -1.0f;
    config.limits.positionMaximum = 1.0f;
    ConstructSubjectWithConfig(config);

    EXPECT_TRUE(subject->Commands().TrySetPosition(foc::Radians{ 2.0f }));

    EXPECT_NEAR(subject->Drive().ActiveStateMachine().ObserveMotion().positionError.Value(), 1.0f, 1e-6f);
}

TEST_F(ControlModeStateMachineExtTest, travel_limits_resetStoredlater_reach_the_position_reference)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    services::ConfigData config{};
    config.defaultControlMode = static_cast<uint8_t>(state_machine::ControlMode::position);
    ConstructSubjectWithConfig(config);
    EXPECT_TRUE(subject->Commands().TrySetPosition(foc::Radians{ 2.0f }));
    EXPECT_NEAR(subject->Drive().ActiveStateMachine().ObserveMotion().positionError.Value(), 2.0f, 1e-6f);

    subject->Config().limits.positionMinimum = -0.5f;
    subject->Config().limits.positionMaximum = 0.5f;
    subject->PersistConfig([](services::NvmStatus) {});

    EXPECT_NEAR(subject->Drive().ActiveStateMachine().ObserveMotion().positionError.Value(), 0.5f, 1e-6f);
}

TEST_F(ControlModeStateMachineExtTest, the_winding_temperature_is_unknown_in_torque_mode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_FALSE(subject->Drive().WindingTemperature().has_value());
}

TEST_F(ControlModeStateMachineExtTest, the_winding_temperature_is_unknown_without_a_calibrated_resistance)
{
    GivenNvmAlwaysInvalid();
    services::ConfigData config{};
    config.defaultControlMode = static_cast<uint8_t>(state_machine::ControlMode::speed);
    ConstructSubjectWithConfig(config);

    EXPECT_FALSE(subject->Drive().WindingTemperature().has_value());
}

TEST_F(ControlModeStateMachineExtTest, TrySetPosition_Returns_True_When_Position_Mode_Active)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    EXPECT_TRUE(subject->Commands().TrySetPosition(foc::Radians{ 1.0f }));
}

TEST_F(ControlModeStateMachineExtTest, TrySetTorque_Returns_False_When_Position_Active)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    EXPECT_FALSE(subject->Commands().TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } }));
}

TEST_F(ControlModeStateMachineExtTest, ActiveStateMachine_Is_Accessible_In_Speed_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    // CmdDisable is a no-op in Idle — just verifies no crash and correct SM is returned
    subject->Drive().ActiveStateMachine().CmdDisable();
}

TEST_F(ControlModeStateMachineExtTest, ActiveStateMachine_Is_Accessible_In_Position_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    subject->Drive().ActiveStateMachine().CmdDisable();
}

TEST_F(ControlModeStateMachineExtTest, Select_Returns_Busy_When_Motor_Is_Enabled)
{
    GivenNvmValid();
    ConstructSubject();
    AlignAfterBoot();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();

    state_machine::SelectResult result{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::busy);
}

TEST_F(ControlModeStateMachineExtTest, Select_From_Aligned_Ready_Keeps_The_Rotor_Reference)
{
    GivenNvmAlwaysValidWithMechanics();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();
    AlignAfterBoot();

    state_machine::SelectResult result{ state_machine::SelectResult::busy };
    subject->Modes().Select(state_machine::ControlMode::speed, [&result](state_machine::SelectResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::SelectResult::ok);
    EXPECT_TRUE(IsReadyWithRotorReference());

    EXPECT_CALL(inverterMock, Start());
    EXPECT_EQ(subject->Drive().ActiveStateMachine().CmdEnable(), state_machine::CommandResult::ok);
}

TEST_F(ControlModeStateMachineExtTest, Select_From_Unaligned_Idle_Leaves_The_New_Mode_To_Be_Aligned)
{
    GivenNvmAlwaysValidWithMechanics();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::position, [](state_machine::SelectResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(subject->Drive().ActiveStateMachine().CurrentState()));
    EXPECT_EQ(subject->Drive().ActiveStateMachine().CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(ControlModeStateMachineExtTest, Select_Into_A_Mode_The_Record_Does_Not_Complete_Stays_Idle)
{
    GivenNvmAlwaysValid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();
    AlignAfterBoot();

    subject->Modes().Select(state_machine::ControlMode::speed, [](state_machine::SelectResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineExtTest, A_Mode_Left_Idle_By_A_Switch_Has_No_Rotor_Reference_Once_Its_Record_Completes)
{
    GivenNvmAlwaysValid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();
    AlignAfterBoot();
    subject->Modes().Select(state_machine::ControlMode::speed, [](state_machine::SelectResult) {});

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .Times(2)
        .WillRepeatedly(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    subject->Tunings().SetMotorParameter(services::MotorParameter::inertia, 1.0e-5f, [](state_machine::SettingResult) {});
    subject->Tunings().SetMotorParameter(services::MotorParameter::viscousFriction, 1.0e-4f, [](state_machine::SettingResult) {});
    ASSERT_EQ(subject->Drive().ActiveCommon().Commissioning().CmdReserveExternalCalibration(), state_machine::CommandResult::ok);
    ASSERT_EQ(subject->Drive().ActiveCommon().Commissioning().CmdReleaseExternalCalibration(), state_machine::CommandResult::ok);

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(subject->Drive().ActiveStateMachine().CurrentState()));
    EXPECT_FALSE(IsReadyWithRotorReference());
}

namespace
{
    class ControlModeStateMachineLifecycleTest
        : public ControlModeStateMachineExtTest
    {
    public:
        infra::Function<void(std::optional<std::size_t>)> capturedPolePairsCb;
        infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> capturedResistanceCb;
        infra::Function<void(std::optional<foc::Radians>)> capturedAlignmentCb;
        infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>,
            std::optional<foc::NewtonMeterSecondSquared>)>
            capturedMechIdentCb;
        infra::Function<void(services::NvmStatus)> capturedNvmSaveCb;

        void SetUpTorqueCalibrationCaptures()
        {
            EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                .WillOnce(Invoke([this](const auto&, const auto& cb)
                    {
                        capturedPolePairsCb = cb;
                    }));
            EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                .WillOnce(Invoke([this](const auto&, const auto& cb)
                    {
                        capturedResistanceCb = cb;
                    }));
            EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                .WillOnce(Invoke([this](auto, const auto&, const auto& cb)
                    {
                        capturedAlignmentCb = cb;
                    }));
            EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                .WillOnce(Invoke([this](const auto&, const auto& cb)
                    {
                        capturedNvmSaveCb = cb;
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(0);
        }

        void SetUpMechIdentCalibrationCaptures()
        {
            SetUpTorqueCalibrationCaptures();
            EXPECT_CALL(mechIdentMock, EstimateFrictionAndInertia(_, _, _, _))
                .WillOnce(Invoke([this](const auto&, auto, const auto&, const auto& cb)
                    {
                        capturedMechIdentCb = cb;
                    }));
        }

        void CompleteCalibration_Torque()
        {
            capturedPolePairsCb(7);
            capturedResistanceCb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            capturedAlignmentCb(foc::Radians{ 0.0f });
            capturedNvmSaveCb(services::NvmStatus::Ok);
        }

        void CompleteCalibration_WithMechIdent()
        {
            capturedPolePairsCb(7);
            capturedResistanceCb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            capturedAlignmentCb(foc::Radians{ 0.0f });
            capturedMechIdentCb(foc::NewtonMeterSecondPerRadian{ 0.005f }, foc::NewtonMeterSecondSquared{ 0.01f });
            capturedNvmSaveCb(services::NvmStatus::Ok);
        }
    };
}

TEST_F(ControlModeStateMachineLifecycleTest, Full_Calibration_Completes_To_Ready_In_Torque_Mode)
{
    GivenNvmAlwaysInvalid();
    SetUpTorqueCalibrationCaptures();
    ConstructSubject();

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_Torque();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Algorithm_Select_Mid_Calibration_Is_Refused_And_Calibration_Still_Completes)
{
    GivenNvmAlwaysInvalid();
    SetUpTorqueCalibrationCaptures();
    ConstructSubject();

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(
        subject->Drive().ActiveStateMachine().CurrentState()));

    EXPECT_EQ(subject->Algorithms().SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::busy);

    CompleteCalibration_Torque();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, CmdEnable_And_Disable_From_Ready_In_Torque_Mode)
{
    GivenNvmAlwaysInvalid();
    SetUpTorqueCalibrationCaptures();
    ConstructSubject();

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_Torque();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(
        subject->Drive().ActiveStateMachine().CurrentState()));

    subject->Drive().ActiveStateMachine().CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Fault_And_ClearFault_In_Torque_Mode)
{
    GivenNvmAlwaysInvalid();
    SetUpTorqueCalibrationCaptures();
    ConstructSubject();

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_Torque();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(
        subject->Drive().ActiveStateMachine().CurrentState()));
    EXPECT_EQ(subject->Drive().ActiveStateMachine().LastFaultCode(), state_machine::FaultCode::hardwareFault);

    subject->Drive().ActiveStateMachine().CmdClearFault();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Full_Calibration_Completes_To_Ready_In_Speed_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, CmdEnable_And_Disable_From_Ready_In_Speed_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(
        subject->Drive().ActiveStateMachine().CurrentState()));

    subject->Drive().ActiveStateMachine().CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Fault_And_ClearFault_In_Speed_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(
        subject->Drive().ActiveStateMachine().CurrentState()));
    EXPECT_EQ(subject->Drive().ActiveStateMachine().LastFaultCode(), state_machine::FaultCode::hardwareFault);

    subject->Drive().ActiveStateMachine().CmdClearFault();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Recalibrate_Is_Refused_While_Enabled_In_Speed_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();

    subject->Config().recalibrationMode = static_cast<uint8_t>(services::RecalibrationMode::onCommand);

    EXPECT_EQ(state_machine::SettingResult::invalidState, subject->Recalibration().CmdRecalibrate());
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Recalibrate_With_The_Mode_Off_Is_Refused)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});
    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_EQ(state_machine::SettingResult::invalidState, subject->Recalibration().CmdRecalibrate());
}

TEST_F(ControlModeStateMachineLifecycleTest, Recalibrate_While_Stopped_With_Nothing_Settled_Keeps_The_Calibration_Record)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});
    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();
    subject->Config().recalibrationMode = static_cast<uint8_t>(services::RecalibrationMode::onCommand);

    EXPECT_EQ(state_machine::SettingResult::ok, subject->Recalibration().CmdRecalibrate());

    const auto& record = subject->Drive().CalibrationRecord();
    ASSERT_TRUE(subject->Recalibration().ModelInUse().has_value());
    EXPECT_EQ((state_machine::MotorModel{ foc::Ohm{ record.rPhase }, foc::MilliHenry{ record.lD }, foc::NewtonMeterSecondSquared{ record.inertia }, foc::NewtonMeterSecondPerRadian{ record.frictionViscous } }), *subject->Recalibration().ModelInUse());
    EXPECT_EQ(0, subject->Recalibration().RecalibrationsApplied());
    EXPECT_EQ(0, subject->Recalibration().RecalibrationsRejected());
}

TEST_F(ControlModeStateMachineLifecycleTest, Torque_Mode_Has_No_Model_In_Use)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_FALSE(subject->Recalibration().ModelInUse().has_value());
}

namespace
{
    state_machine::MotorModel ModelOf(const services::CalibrationData& record)
    {
        return state_machine::MotorModel{ foc::Ohm{ record.rPhase }, foc::MilliHenry{ record.lD }, foc::NewtonMeterSecondSquared{ record.inertia }, foc::NewtonMeterSecondPerRadian{ record.frictionViscous } };
    }
}

TEST_F(ControlModeStateMachineLifecycleTest, A_Stored_Inertia_Learned_From_The_Record_Is_In_Use_At_Ready)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});
    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();
    subject->Config().recalibrationMode = static_cast<uint8_t>(services::RecalibrationMode::automatic);
    const auto record = ModelOf(subject->Drive().CalibrationRecord());
    const services::LearnedModelData stored{ state_machine::CommissioningCheck(record), std::nullopt, 1.5f * record.inertia.Value(), std::nullopt };
    EXPECT_CALL(nvmMock, LoadLearnedModel(_, _)).WillOnce(Invoke([stored](services::LearnedModelData& model, const infra::Function<void(services::NvmStatus)>& onDone)
        {
            model = stored;
            onDone(services::NvmStatus::Ok);
        }));

    subject->Recalibration().RestoreStoredModel();

    ASSERT_TRUE(subject->Recalibration().ModelInUse().has_value());
    EXPECT_NEAR(1.5f * record.inertia.Value(), subject->Recalibration().ModelInUse()->inertia.Value(), 1e-12f);
    EXPECT_EQ(record.inductance, subject->Recalibration().ModelInUse()->inductance);
    EXPECT_EQ(0, subject->Recalibration().RecalibrationsStored());
}

TEST_F(ControlModeStateMachineLifecycleTest, With_The_Mode_Off_A_Stored_Model_Is_Not_Used)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});
    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();
    const auto record = ModelOf(subject->Drive().CalibrationRecord());
    const services::LearnedModelData stored{ state_machine::CommissioningCheck(record), std::nullopt, 1.5f * record.inertia.Value(), std::nullopt };
    EXPECT_CALL(nvmMock, LoadLearnedModel(_, _)).WillOnce(Invoke([stored](services::LearnedModelData& model, const infra::Function<void(services::NvmStatus)>& onDone)
        {
            model = stored;
            onDone(services::NvmStatus::Ok);
        }));

    subject->Recalibration().RestoreStoredModel();

    ASSERT_TRUE(subject->Recalibration().ModelInUse().has_value());
    EXPECT_EQ(record, *subject->Recalibration().ModelInUse());
}

TEST_F(ControlModeStateMachineLifecycleTest, ResetModel_Is_Refused_While_Enabled)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});
    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();
    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();

    EXPECT_EQ(state_machine::SettingResult::invalidState, subject->Recalibration().CmdResetModel([](bool) {}));
}

TEST_F(ControlModeStateMachineLifecycleTest, ResetModel_While_Stopped_Stores_An_Empty_Model_And_Returns_To_The_Record)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});
    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();
    subject->Config().recalibrationMode = static_cast<uint8_t>(services::RecalibrationMode::onCommand);
    const auto record = ModelOf(subject->Drive().CalibrationRecord());
    const services::LearnedModelData stored{ state_machine::CommissioningCheck(record), std::nullopt, 1.5f * record.inertia.Value(), std::nullopt };
    EXPECT_CALL(nvmMock, LoadLearnedModel(_, _)).WillOnce(Invoke([stored](services::LearnedModelData& model, const infra::Function<void(services::NvmStatus)>& onDone)
        {
            model = stored;
            onDone(services::NvmStatus::Ok);
        }));
    subject->Recalibration().RestoreStoredModel();
    EXPECT_CALL(nvmMock, SaveLearnedModel(services::LearnedModelData{}, _)).WillOnce(Invoke([](const services::LearnedModelData&, const infra::Function<void(services::NvmStatus)>& onDone)
        {
            onDone(services::NvmStatus::Ok);
        }));
    std::optional<bool> resetStored;

    EXPECT_EQ(state_machine::SettingResult::ok, subject->Recalibration().CmdResetModel([&resetStored](bool result)
                                                    {
                                                        resetStored = result;
                                                    }));

    EXPECT_EQ(true, resetStored);
    ASSERT_TRUE(subject->Recalibration().ModelInUse().has_value());
    EXPECT_EQ(record, *subject->Recalibration().ModelInUse());
    EXPECT_EQ(1, subject->Recalibration().RecalibrationsStored());
}

TEST_F(ControlModeStateMachineLifecycleTest, Full_Calibration_Completes_To_Ready_In_Position_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, CmdEnable_And_Disable_From_Ready_In_Position_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(
        subject->Drive().ActiveStateMachine().CurrentState()));

    subject->Drive().ActiveStateMachine().CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineLifecycleTest, Fault_And_ClearFault_In_Position_Mode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(
        subject->Drive().ActiveStateMachine().CurrentState()));
    EXPECT_EQ(subject->Drive().ActiveStateMachine().LastFaultCode(), state_machine::FaultCode::hardwareFault);

    subject->Drive().ActiveStateMachine().CmdClearFault();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(
        subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineExtTest, TrySetCurrentBandwidth_AcceptedInTorqueMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([](const services::ConfigData& stored, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(stored.currentLoop.bandwidth, 8377.6f, 1e-2f);
                onDone(services::NvmStatus::Ok);
            }));

    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(8377.6f), state_machine::TuningResult::ok);
    EXPECT_NEAR(subject->Tunings().StoredCurrentTunings().bandwidth, 8377.6f, 1e-2f);
}

TEST_F(ControlModeStateMachineExtTest, TrySetSpeedBandwidth_RejectedInTorqueMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->Tunings().TrySetSpeedBandwidth(50.0f), state_machine::TuningResult::wrongMode);
}

TEST_F(ControlModeStateMachineExtTest, TrySetSpeedBandwidth_AcceptedInSpeedMode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    EXPECT_EQ(subject->Tunings().TrySetSpeedBandwidth(50.0f), state_machine::TuningResult::ok);
}

TEST_F(ControlModeStateMachineExtTest, TrySetPositionBandwidth_AcceptedOnlyInPositionMode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();

    EXPECT_EQ(subject->Tunings().TrySetPositionBandwidth(18.8f), state_machine::TuningResult::wrongMode);

    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    EXPECT_EQ(subject->Tunings().TrySetPositionBandwidth(18.8f), state_machine::TuningResult::ok);
}

TEST_F(ControlModeStateMachineExtTest, TrySetCurrentBandwidth_RejectsValuesOutsideTheSharedLimits)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(0.0f), state_machine::TuningResult::outOfRange);
    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(-6283.2f), state_machine::TuningResult::outOfRange);
    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(foc::CommandLimits::maxCurrentBandwidth + 1.0f), state_machine::TuningResult::outOfRange);
}

TEST_F(ControlModeStateMachineExtTest, TrySetBandwidth_RejectsNonFiniteValuesOnEveryLoop)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(NotANumber()), state_machine::TuningResult::outOfRange);
    EXPECT_EQ(subject->Tunings().TrySetSpeedBandwidth(NotANumber()), state_machine::TuningResult::outOfRange);
    EXPECT_EQ(subject->Tunings().TrySetPositionBandwidth(NotANumber()), state_machine::TuningResult::outOfRange);

    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(Infinity()), state_machine::TuningResult::outOfRange);
    EXPECT_EQ(subject->Tunings().TrySetSpeedBandwidth(Infinity()), state_machine::TuningResult::outOfRange);
    EXPECT_EQ(subject->Tunings().TrySetPositionBandwidth(Infinity()), state_machine::TuningResult::outOfRange);
}

TEST_F(ControlModeStateMachineExtTest, TrySetBandwidth_RejectsAMisrangedValueBeforeCheckingTheMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->Tunings().TrySetSpeedBandwidth(-1.0f), state_machine::TuningResult::outOfRange);
}

TEST_F(ControlModeStateMachineExtTest, TrySetCurrentBandwidth_IsRejectedWhileEnabled)
{
    GivenNvmValid();
    ConstructSubject();
    AlignAfterBoot();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();

    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(6283.2f), state_machine::TuningResult::notStopped);
}

TEST_F(ControlModeStateMachineExtTest, TrySetCurrentBandwidth_IsAcceptedAgainOnceDisabled)
{
    GivenNvmValid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();
    AlignAfterBoot();

    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();
    ASSERT_EQ(subject->Tunings().TrySetCurrentBandwidth(6283.2f), state_machine::TuningResult::notStopped);

    EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
    subject->Drive().ActiveStateMachine().CmdDisable();

    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(6283.2f), state_machine::TuningResult::ok);
}

TEST_F(ControlModeStateMachineExtTest, SelectCurrentAlgorithm_DefaultsToPid)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->Algorithms().ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(ControlModeStateMachineExtTest, SelectCurrentAlgorithm_RejectedWithoutMotorModel)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->Algorithms().SelectCurrentAlgorithm(foc::CurrentAlgorithm::deadbeat), foc::SelectResult::invalidParameters);
    EXPECT_EQ(subject->Algorithms().ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(ControlModeStateMachineExtTest, SelectCurrentAlgorithm_PersistsToNvmOnceCalibrated)
{
    GivenNvmValid();
    ConstructSubject();

    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([](const services::ConfigData& config, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_EQ(config.currentAlgorithm, static_cast<uint8_t>(foc::CurrentAlgorithm::slidingMode));
                onDone(services::NvmStatus::Ok);
            }));

    EXPECT_EQ(subject->Algorithms().SelectCurrentAlgorithm(foc::CurrentAlgorithm::slidingMode), foc::SelectResult::ok);
    EXPECT_EQ(subject->Algorithms().ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::slidingMode);
}

TEST_F(ControlModeStateMachineExtTest, PersistedAlgorithmOutsideEnumRangeFallsBackToDefault)
{
    GivenNvmValid();

    services::ConfigData config{};
    config.currentAlgorithm = 200;
    config.speedAlgorithm = 200;
    config.positionAlgorithm = 200;
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->Algorithms().ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(ControlModeStateMachineExtTest, ActiveAlgorithmReportsTheRunningLoopNotThePersistedByte)
{
    GivenNvmAlwaysInvalid();

    services::ConfigData config{};
    config.currentAlgorithm = static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat);
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->Algorithms().ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);
}

TEST_F(ControlModeStateMachineExtTest, SelectSpeedAlgorithm_RejectedInTorqueMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->Algorithms().SelectSpeedAlgorithm(foc::SpeedAlgorithm::lqi), foc::SelectResult::invalidAlgorithm);
}

TEST_F(ControlModeStateMachineExtTest, SelectSpeedAlgorithm_RejectedWithoutMechanicalModel)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    EXPECT_EQ(subject->Algorithms().SelectSpeedAlgorithm(foc::SpeedAlgorithm::lqi), foc::SelectResult::invalidParameters);
}

TEST_F(ControlModeStateMachineExtTest, Cli_SelectPositionAlgorithm_RejectedOutsidePositionMode)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_EQ(subject->Algorithms().SelectPositionAlgorithm(foc::PositionAlgorithm::lqr), foc::SelectResult::invalidAlgorithm);
}

TEST_F(ControlModeStateMachineExtTest, TrySetBandwidths_AreAcceptedInPositionMode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(6283.2f), state_machine::TuningResult::ok);
    EXPECT_EQ(subject->Tunings().TrySetSpeedBandwidth(188.5f), state_machine::TuningResult::ok);
    EXPECT_EQ(subject->Tunings().TrySetPositionBandwidth(18.8f), state_machine::TuningResult::ok);
}

TEST_F(ControlModeStateMachineExtTest, TrySetSpeedAndPositionBandwidth_AreRejectedInTorqueMode)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    ConstructSubject();

    EXPECT_EQ(subject->Tunings().TrySetCurrentBandwidth(6283.2f), state_machine::TuningResult::ok);
    EXPECT_EQ(subject->Tunings().TrySetSpeedBandwidth(188.5f), state_machine::TuningResult::wrongMode);
    EXPECT_EQ(subject->Tunings().TrySetPositionBandwidth(18.8f), state_machine::TuningResult::wrongMode);
}

TEST_F(ControlModeStateMachineExtTest, PersistedAlgorithmIsKeptInConfigWhileStillUnselectable)
{
    GivenNvmAlwaysInvalid();

    services::ConfigData config{};
    config.currentAlgorithm = static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat);
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->Algorithms().ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);

    uint8_t persisted{ 0 };
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&persisted](const services::ConfigData& saved, infra::Function<void(services::NvmStatus)> onDone)
            {
                persisted = saved.currentAlgorithm;
                onDone(services::NvmStatus::Ok);
            }));

    subject->Modes().Select(state_machine::ControlMode::torque, [](auto) {});

    EXPECT_EQ(persisted, static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat));
}

TEST_F(ControlModeStateMachineExtTest, PersistedAlgorithmOutsideEnumRangeIsCorrectedInConfig)
{
    GivenNvmAlwaysValid();

    services::ConfigData config{};
    config.currentAlgorithm = 200;
    ConstructSubjectWithConfig(config);
    AlignAfterBoot();

    uint8_t persisted{ 200 };
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&persisted](const services::ConfigData& saved, infra::Function<void(services::NvmStatus)> onDone)
            {
                persisted = saved.currentAlgorithm;
                onDone(services::NvmStatus::Ok);
            }));

    subject->Modes().Select(state_machine::ControlMode::torque, [](auto) {});

    EXPECT_EQ(persisted, static_cast<uint8_t>(foc::CurrentAlgorithm::pid));
}

TEST_F(ControlModeStateMachineExtTest, StoredTunings_AreRestoredFromTheConfigurationRecord)
{
    GivenNvmAlwaysInvalid();

    services::ConfigData config{};
    config.currentLoop.bandwidth = 5000.0f;
    config.currentLoop.switchingGain = 0.3f;
    config.currentLoop.boundaryLayer = 0.9f;
    config.speedLoop.observerBandwidthRatio = 8.0f;
    ConstructSubjectWithConfig(config);

    EXPECT_NEAR(subject->Tunings().StoredCurrentTunings().bandwidth, 5000.0f, 1e-2f);
    EXPECT_NEAR(subject->Tunings().StoredCurrentTunings().switchingGain, 0.3f, 1e-6f);
    EXPECT_NEAR(subject->Tunings().StoredCurrentTunings().boundaryLayer, 0.9f, 1e-6f);
    EXPECT_NEAR(subject->Tunings().StoredSpeedTunings().observerBandwidthRatio, 8.0f, 1e-5f);
    EXPECT_NEAR(subject->Tunings().EffectiveCurrentTunings().bandwidth, 5000.0f, 1e-2f);
}

TEST_F(ControlModeStateMachineExtTest, StoredTunings_ThatCannotBeUsed_FallBackToTheDefaults)
{
    GivenNvmAlwaysInvalid();

    services::ConfigData config{};
    config.currentLoop.switchingGain = 0.9f;
    config.speedLoop.observerBandwidthRatio = 90.0f;
    ConstructSubjectWithConfig(config);

    EXPECT_NEAR(subject->Tunings().StoredCurrentTunings().switchingGain, foc::CurrentLoopTunings{}.switchingGain, 1e-6f);
    EXPECT_NEAR(subject->Tunings().StoredSpeedTunings().observerBandwidthRatio, foc::SpeedLoopTunings{}.observerBandwidthRatio, 1e-6f);
}

TEST_F(ControlModeStateMachineExtTest, EffectiveTunings_OfLoopsTheModeDoesNotRun_AreUnavailable)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_FALSE(subject->Tunings().EffectiveSpeedTunings().has_value());
    EXPECT_FALSE(subject->Tunings().EffectivePositionTunings().has_value());
}

TEST_F(ControlModeStateMachineExtTest, SetCurrentTunings_IsRefusedWhileEnabledAndNothingIsStored)
{
    GivenNvmValid();
    ConstructSubject();
    AlignAfterBoot();
    EXPECT_CALL(inverterMock, Start()).Times(1);
    subject->Drive().ActiveStateMachine().CmdEnable();

    auto tunings = subject->Tunings().StoredCurrentTunings();
    tunings.boundaryLayer = 0.9f;
    std::optional<state_machine::SettingResult> result;
    subject->Tunings().SetCurrentTunings(tunings, [&result](state_machine::SettingResult value)
        {
            result = value;
        });

    EXPECT_EQ(state_machine::SettingResult::invalidState, result);
    EXPECT_NEAR(subject->Tunings().StoredCurrentTunings().boundaryLayer, foc::CurrentLoopTunings{}.boundaryLayer, 1e-6f);
}

TEST_F(ControlModeStateMachineExtTest, SetCurrentTunings_AnswersOnlyOnceTheRecordIsStored)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    infra::Function<void(services::NvmStatus)> pendingSave;
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&pendingSave](const services::ConfigData& saved, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(saved.currentLoop.boundaryLayer, 0.9f, 1e-6f);
                pendingSave = onDone;
            }));

    auto tunings = subject->Tunings().StoredCurrentTunings();
    tunings.boundaryLayer = 0.9f;
    std::optional<state_machine::SettingResult> result;
    subject->Tunings().SetCurrentTunings(tunings, [&result](state_machine::SettingResult value)
        {
            result = value;
        });

    EXPECT_FALSE(result.has_value());
    pendingSave(services::NvmStatus::Ok);
    EXPECT_EQ(state_machine::SettingResult::ok, result);
}

TEST_F(ControlModeStateMachineExtTest, SetMotorParameter_OutsideItsRange_IsRefusedWithoutAWrite)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    std::optional<state_machine::SettingResult> result;
    const auto record = [&result](state_machine::SettingResult value)
    {
        result = value;
    };

    subject->Tunings().SetMotorParameter(services::MotorParameter::polePairs, 0.0f, record);
    EXPECT_EQ(state_machine::SettingResult::outOfRange, result);

    subject->Tunings().SetMotorParameter(services::MotorParameter::resistance, -1.0f, record);
    EXPECT_EQ(state_machine::SettingResult::outOfRange, result);

    subject->Tunings().SetMotorParameter(services::MotorParameter::inertia, 100.0f, record);
    EXPECT_EQ(state_machine::SettingResult::outOfRange, result);

    subject->Tunings().SetMotorParameter(services::MotorParameter::frictionCoulomb, -1.0e-3f, record);
    EXPECT_EQ(state_machine::SettingResult::outOfRange, result);

    subject->Tunings().SetMotorParameter(services::MotorParameter::frictionCoulomb, foc::CommandLimits::maxFrictionCoulomb * 2.0f, record);
    EXPECT_EQ(state_machine::SettingResult::outOfRange, result);
}

TEST_F(ControlModeStateMachineExtTest, SetMotorParameter_InterruptedByAnEmergencyStop_IsAnsweredAsAborted)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    infra::Function<void(services::NvmStatus)> pendingSave;
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&pendingSave](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                pendingSave = onDone;
            }));

    std::optional<state_machine::SettingResult> result;
    subject->Tunings().SetMotorParameter(services::MotorParameter::inertia, 2.0e-5f, [&result](state_machine::SettingResult value)
        {
            result = value;
        });
    ASSERT_FALSE(result.has_value());

    EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
    subject->Drive().ActiveStateMachine().CmdEmergencyStop();

    EXPECT_EQ(state_machine::SettingResult::aborted, result);
    pendingSave(services::NvmStatus::Ok);
}

TEST_F(ControlModeStateMachineExtTest, SelectCurrentAlgorithm_WithACallback_AnswersOnceTheSelectionIsStored)
{
    GivenNvmValid();
    ConstructSubject();
    AlignAfterBoot();

    infra::Function<void(services::NvmStatus)> pendingSave;
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&pendingSave](const services::ConfigData& saved, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_EQ(saved.currentAlgorithm, static_cast<uint8_t>(foc::CurrentAlgorithm::slidingMode));
                pendingSave = onDone;
            }));

    std::optional<state_machine::SettingResult> result;
    subject->Algorithms().SelectCurrentAlgorithm(foc::CurrentAlgorithm::slidingMode, [&result](state_machine::SettingResult value)
        {
            result = value;
        });

    EXPECT_EQ(subject->Algorithms().ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::slidingMode);
    EXPECT_FALSE(result.has_value());

    pendingSave(services::NvmStatus::Ok);
    EXPECT_EQ(state_machine::SettingResult::ok, result);
}

TEST_F(ControlModeStateMachineLifecycleTest, PersistedCurrentAlgorithmIsAppliedOnceReadyIsReached)
{
    GivenNvmAlwaysInvalid();
    SetUpTorqueCalibrationCaptures();

    services::ConfigData config{};
    config.currentAlgorithm = static_cast<uint8_t>(foc::CurrentAlgorithm::slidingMode);
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->Algorithms().ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::pid);

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_Torque();

    EXPECT_EQ(subject->Algorithms().ActiveCurrentAlgorithm(), foc::CurrentAlgorithm::slidingMode);
}

TEST_F(ControlModeStateMachineLifecycleTest, PersistedSpeedAlgorithmIsAppliedOnceReadyIsReached)
{
    GivenNvmAlwaysInvalid();
    GivenNvmSaveConfigAlwaysSucceeds();
    SetUpMechIdentCalibrationCaptures();

    services::ConfigData config{};
    config.defaultControlMode = static_cast<uint8_t>(state_machine::ControlMode::speed);
    config.speedAlgorithm = static_cast<uint8_t>(foc::SpeedAlgorithm::lqi);
    ConstructSubjectWithConfig(config);

    EXPECT_EQ(subject->Algorithms().ActiveSpeedAlgorithm(), foc::SpeedAlgorithm::pid);

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();

    EXPECT_EQ(subject->Algorithms().ActiveSpeedAlgorithm(), foc::SpeedAlgorithm::lqi);
}

TEST_F(ControlModeStateMachineTest, IsIdle_While_Idle_With_Nothing_Outstanding)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_TRUE(subject->Settings().IsIdle());
}

TEST_F(ControlModeStateMachineTest, IsIdle_Is_False_While_A_Select_Nvm_Write_Is_In_Flight)
{
    GivenNvmAlwaysInvalid();
    infra::Function<void(services::NvmStatus)> deferredSave;
    EXPECT_CALL(nvmMock, SaveConfig(_, _))
        .WillOnce(Invoke([&deferredSave](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                deferredSave = onDone;
            }));
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::speed, [](state_machine::SelectResult) {});
    EXPECT_FALSE(subject->Settings().IsIdle());

    deferredSave(services::NvmStatus::Ok);
    EXPECT_TRUE(subject->Settings().IsIdle());
}

TEST_F(ControlModeStateMachineTest, Held_Async_Work_Refuses_Calibrate_And_Select_Until_Released)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    subject->HoldAsyncWork();
    EXPECT_FALSE(subject->Settings().IsIdle());

    state_machine::CommandResult calibrateResult{ state_machine::CommandResult::ok };
    subject->Drive().ActiveStateMachine().CmdCalibrate([&calibrateResult](state_machine::CommandResult r)
        {
            calibrateResult = r;
        });
    EXPECT_EQ(calibrateResult, state_machine::CommandResult::rejected);

    state_machine::SelectResult selectResult{ state_machine::SelectResult::ok };
    subject->Modes().Select(state_machine::ControlMode::speed, [&selectResult](state_machine::SelectResult r)
        {
            selectResult = r;
        });
    EXPECT_EQ(selectResult, state_machine::SelectResult::busy);

    subject->ReleaseAsyncWork();
    EXPECT_TRUE(subject->Settings().IsIdle());
}

TEST_F(ControlModeStateMachineExtTest, A_Plain_Alignment_Does_Not_Home_The_Drive)
{
    GivenNvmAlwaysValidWithMechanics();
    ConstructSubject();
    AlignAfterBoot();

    EXPECT_TRUE(IsReadyWithRotorReference());
    EXPECT_FALSE(subject->Drive().IsHomed());
}

TEST_F(ControlModeStateMachineExtTest, Position_Mode_Is_Refused_Without_Homing_When_Homing_Is_Required)
{
    GivenNvmAlwaysValidWithMechanics();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();
    AlignAfterBoot();
    subject->Config().homingRequired = 1;
    subject->Modes().Select(state_machine::ControlMode::position, [](state_machine::SelectResult) {});

    ASSERT_TRUE(IsReadyWithRotorReference());
    EXPECT_EQ(subject->Drive().ActiveStateMachine().CmdEnable(), state_machine::CommandResult::rejected);
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(subject->Drive().ActiveStateMachine().CurrentState()));
}

TEST_F(ControlModeStateMachineExtTest, Position_Mode_Is_Enabled_Without_Homing_When_Homing_Is_Not_Required)
{
    GivenNvmAlwaysValidWithMechanics();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();
    AlignAfterBoot();
    subject->Modes().Select(state_machine::ControlMode::position, [](state_machine::SelectResult) {});

    EXPECT_CALL(inverterMock, Start());
    EXPECT_EQ(subject->Drive().ActiveStateMachine().CmdEnable(), state_machine::CommandResult::ok);
}

TEST_F(ControlModeStateMachineExtTest, Speed_Mode_Is_Never_Held_Back_By_Required_Homing)
{
    GivenNvmAlwaysValidWithMechanics();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();
    AlignAfterBoot();
    subject->Config().homingRequired = 1;
    subject->Modes().Select(state_machine::ControlMode::speed, [](state_machine::SelectResult) {});

    EXPECT_CALL(inverterMock, Start());
    EXPECT_EQ(subject->Drive().ActiveStateMachine().CmdEnable(), state_machine::CommandResult::ok);
}

TEST_F(ControlModeStateMachineExtTest, Torque_Mode_Is_Never_Held_Back_By_Required_Homing)
{
    GivenNvmAlwaysValidWithMechanics();
    GivenNvmSaveConfigSucceeds();
    ConstructSubject();
    AlignAfterBoot();
    subject->Config().homingRequired = 1;
    subject->PersistConfig([](services::NvmStatus) {});

    EXPECT_CALL(inverterMock, Start());
    EXPECT_EQ(subject->Drive().ActiveStateMachine().CmdEnable(), state_machine::CommandResult::ok);
}

TEST_F(ControlModeStateMachineLifecycleTest, A_Calibration_Homes_The_Drive_And_A_Mode_Switch_Keeps_It_Homed)
{
    GivenNvmAlwaysValidWithMechanics();
    GivenNvmSaveConfigAlwaysSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});
    ASSERT_FALSE(subject->Drive().IsHomed());

    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(subject->Drive().ActiveStateMachine().CurrentState()));
    EXPECT_TRUE(subject->Drive().IsHomed());

    subject->Config().homingRequired = 1;
    subject->Modes().Select(state_machine::ControlMode::position, [](auto) {});

    EXPECT_TRUE(subject->Drive().IsHomed());
    EXPECT_CALL(inverterMock, Start());
    EXPECT_EQ(subject->Drive().ActiveStateMachine().CmdEnable(), state_machine::CommandResult::ok);
}

TEST_F(ControlModeStateMachineLifecycleTest, A_Re_Alignment_After_A_Calibration_Leaves_The_Drive_Not_Homed)
{
    GivenNvmAlwaysValidWithMechanics();
    GivenNvmSaveConfigSucceeds();
    SetUpMechIdentCalibrationCaptures();
    ConstructSubject();
    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});
    subject->Drive().ActiveStateMachine().CmdCalibrate([](state_machine::CommandResult) {});
    CompleteCalibration_WithMechIdent();
    ASSERT_TRUE(subject->Drive().IsHomed());

    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    subject->Drive().ActiveCommon().Commissioning().CmdReAlign([](state_machine::CommandResult) {});
    ExecuteAllActions();

    EXPECT_TRUE(IsReadyWithRotorReference());
    EXPECT_FALSE(subject->Drive().IsHomed());
}

TEST_F(ControlModeStateMachineTest, Torque_Mode_Offers_Only_The_Current_Loop_Controller)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject(static_cast<uint8_t>(state_machine::ControlMode::torque));

    auto& drive = subject->Drive();
    EXPECT_NE(drive.Controller<foc::CurrentLoopTunable>(), nullptr);
    EXPECT_NE(drive.Controller<foc::CurrentLoopSelectable>(), nullptr);
    EXPECT_EQ(drive.Controller<foc::SpeedLoopTunable>(), nullptr);
    EXPECT_EQ(drive.Controller<foc::SpeedLoopSelectable>(), nullptr);
    EXPECT_EQ(drive.Controller<foc::PositionLoopTunable>(), nullptr);
    EXPECT_EQ(drive.ActiveOuterLoop(), nullptr);
}

TEST_F(ControlModeStateMachineTest, Speed_Mode_Offers_The_Current_And_Speed_Loop_Controllers)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject(static_cast<uint8_t>(state_machine::ControlMode::speed));

    const auto& drive = std::as_const(*subject).Drive();
    EXPECT_NE(drive.Controller<foc::CurrentLoopSelectable>(), nullptr);
    EXPECT_NE(drive.Controller<foc::SpeedLoopSelectable>(), nullptr);
    EXPECT_EQ(drive.Controller<foc::PositionLoopSelectable>(), nullptr);
    EXPECT_NE(drive.ActiveOuterLoop(), nullptr);
}

TEST_F(ControlModeStateMachineTest, Position_Mode_Offers_Every_Loop_Controller)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject(static_cast<uint8_t>(state_machine::ControlMode::position));

    auto& drive = subject->Drive();
    EXPECT_NE(drive.Controller<foc::CurrentLoopTunable>(), nullptr);
    EXPECT_NE(drive.Controller<foc::SpeedLoopTunable>(), nullptr);
    EXPECT_NE(drive.Controller<foc::PositionLoopTunable>(), nullptr);
    EXPECT_NE(drive.ActiveOuterLoop(), nullptr);
}

TEST_F(ControlModeStateMachineTest, A_Mode_Switch_In_Flight_Holds_Settings_Off)
{
    GivenNvmAlwaysInvalid();
    infra::Function<void(services::NvmStatus)> saved;
    EXPECT_CALL(nvmMock, SaveConfig(_, _)).WillOnce(Invoke([&saved](const services::ConfigData&, infra::Function<void(services::NvmStatus)> onDone)
        {
            saved = onDone;
        }));
    ConstructSubject();

    subject->Modes().Select(state_machine::ControlMode::speed, [](auto) {});

    EXPECT_TRUE(subject->Modes().IsPending());
    EXPECT_EQ(subject->Settings().Gate(), state_machine::SettingResult::busy);
    EXPECT_TRUE(subject->HasPendingConfigWrite());

    saved(services::NvmStatus::Ok);
    EXPECT_FALSE(subject->Modes().IsPending());
    EXPECT_EQ(subject->Drive().Active(), state_machine::ControlMode::speed);
}

TEST_F(ControlModeStateMachineTest, Torque_Mode_Has_No_Model_To_Recalibrate)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject();

    EXPECT_FALSE(subject->Recalibration().ModelInUse().has_value());
    EXPECT_EQ(subject->Recalibration().CmdRecalibrate(), state_machine::SettingResult::wrongMode);
}

TEST_F(ControlModeStateMachineTest, A_Speed_Setpoint_Is_Clamped_To_The_Overspeed_Limit)
{
    GivenNvmAlwaysInvalid();
    ConstructSubject(static_cast<uint8_t>(state_machine::ControlMode::speed));

    EXPECT_TRUE(subject->Commands().TrySetSpeed(foc::RadiansPerSecond{ 1.0e6f }));
    EXPECT_FALSE(subject->Commands().TrySetTorque(foc::IdAndIqPoint{ foc::Ampere{ 0.0f }, foc::Ampere{ 1.0f } }));
    EXPECT_FALSE(subject->Commands().TrySetPosition(foc::Radians{ 1.0f }));
}
