#include "TestFocStateMachineHelper.hpp"
#include "core/foc/cascade/TorqueCascade.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>

MATCHER_P3(OffsetsNear, a, b, c, "")
{
    return std::abs(arg.a.Value() - a) < 1e-6f && std::abs(arg.b.Value() - b) < 1e-6f && std::abs(arg.c.Value() - c) < 1e-6f;
}

namespace
{
    using namespace testing;

    float NonFiniteFloat(uint32_t bits)
    {
        volatile uint32_t opaque = bits;

        return std::bit_cast<float>(static_cast<uint32_t>(opaque));
    }

    class FocStateMachineTorqueTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        using TestedStateMachine = application::TorqueStateMachine;

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
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        application::NvmActivity nvmActivity;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;
        infra::Execute setupTeardownExpectations{ [this]()
            {
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(electricalIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
            } };

        foc::Volts vdc{ 24.0f };

        infra::Execute setupInverterExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
            } };

        void GivenNvmInvalid()
        {
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([](services::CalibrationData& out, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = services::CalibrationData{};
                        onDone(services::NvmStatus::InvalidData);
                    }));
        }

        static services::CalibrationData ValidRecord()
        {
            services::CalibrationData data{};
            data.polePairs = 7;
            data.rPhase = 0.5f;
            data.lD = 1.0f;
            data.stage = services::CalibrationStage::complete;
            return data;
        }

        void GivenNvmValid()
        {
            GivenNvmHolds(ValidRecord());
        }

        void GivenNvmHolds(const services::CalibrationData& data)
        {
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([data](services::CalibrationData& out,
                                     infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = data;
                        onDone(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
        }

        void GivenFaultNotifierRegistered()
        {
            EXPECT_CALL(faultNotifierMock, Register(_, _))
                .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& immediate, const infra::Function<void(state_machine::FaultCode)>& deferred)
                    {
                        faultNotifierMock.StoreHandler(immediate, deferred);
                    }));
        }

        void GivenNvmHolds(uint8_t polePairs, float rPhase, services::CalibrationStage stage)
        {
            services::CalibrationData data{};
            data.polePairs = polePairs;
            data.rPhase = rPhase;
            data.lD = 1.0f;
            data.stage = stage;

            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([data](services::CalibrationData& out,
                                     infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = data;
                        onDone(services::NvmStatus::Ok);
                    }));
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
        }

        void ExpectCalibrationSequence(bool polePairsOk = true,
            bool resistanceOk = true,
            bool alignmentOk = true,
            bool nvmSaveOk = true)
        {
            if (polePairsOk)
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::size_t{ 7 });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::nullopt);
                        }));
                return;
            }

            if (resistanceOk)
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                        {
                            cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                        {
                            cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{});
                        }));
                return;
            }

            if (alignmentOk)
                EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                    .WillOnce(Invoke([](std::size_t, const auto&,
                                         const infra::Function<void(std::optional<foc::Radians>)>& cb)
                        {
                            cb(foc::Radians{ 0.0f });
                        }));
            else
            {
                EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                    .WillOnce(Invoke([](std::size_t, const auto&,
                                         const infra::Function<void(std::optional<foc::Radians>)>& cb)
                        {
                            cb(std::nullopt);
                        }));
                return;
            }

            if (nvmSaveOk)
            {
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::Ok);
                        }));
                EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
            }
            else
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::WriteFailed);
                        }));
        }

        TestedStateMachine CreateStateMachine()
        {
            return TestedStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock },
                faultNotifierMock,
                nvmActivity
            };
        }

        TestedStateMachine CreateStateMachineIdentifyingFluxLinkage()
        {
            EXPECT_CALL(fluxIdentMock, IsRunning()).WillRepeatedly(Return(false));
            EXPECT_CALL(fluxIdentMock, Abort()).Times(AnyNumber());

            return TestedStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ .electricalIdent = electricalIdentMock, .motorAlignment = alignmentMock, .fluxLinkage = configuredFluxLinkage, .fluxIdentOverride = fluxIdentMock },
                faultNotifierMock,
                nvmActivity
            };
        }

        void ExpectFluxLinkageIdentification(services::FluxLinkageIdentification::Outcome outcome, float fluxLinkage)
        {
            EXPECT_CALL(fluxIdentMock, Identify(7, foc::Ohm{ 0.5f }, _, _))
                .WillOnce(Invoke([outcome, fluxLinkage](std::size_t, foc::Ohm, const auto&, const infra::Function<void(const services::FluxLinkageIdentification::Result&)>& onDone)
                    {
                        services::FluxLinkageIdentification::Result result{};
                        result.outcome = outcome;
                        result.fluxLinkage = foc::Weber{ fluxLinkage };
                        result.samples = 300;
                        onDone(result);
                    }));
        }

        StrictMock<services::FluxLinkageIdentificationMock> fluxIdentMock;
        foc::Weber configuredFluxLinkage{ 0.007f };

        void AlignAfterBoot(TestedStateMachine& sm)
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
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
            sm.CmdReAlign([](state_machine::CommandResult) {});
        }

        StrictMock<services::PhaseCurrentOffsetsMock> offsetsMock;
        StrictMock<services::RotorStandstillMock> standstillMock;
        infra::Function<void(bool)> standstillDone;
        infra::Function<void(std::optional<foc::PhaseCurrents>)> measurementDone;
        std::optional<state_machine::CommandResult> enableResult;

        TestedStateMachine CreateStateMachineRefreshingOffsets()
        {
            EXPECT_CALL(offsetsMock, IsRunning()).WillRepeatedly(Return(false));
            EXPECT_CALL(offsetsMock, Abort()).Times(AnyNumber());
            EXPECT_CALL(standstillMock, Abort()).Times(AnyNumber());

            return TestedStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ .electricalIdent = electricalIdentMock, .motorAlignment = alignmentMock, .currentOffsets = offsetsMock, .standstill = standstillMock },
                faultNotifierMock,
                nvmActivity
            };
        }

        static services::CalibrationData RecordWithOffsets()
        {
            auto data = ValidRecord();
            data.currentOffsetA = 0.010f;
            data.currentOffsetB = -0.020f;
            data.currentOffsetC = 0.030f;
            return data;
        }

        void GivenStoredOffsets()
        {
            GivenFaultNotifierRegistered();
            GivenNvmHolds(RecordWithOffsets());
            EXPECT_CALL(offsetsMock, Apply(OffsetsNear(0.010f, -0.020f, 0.030f))).Times(AtLeast(1));
        }

        void EnableAsync(TestedStateMachine& sm)
        {
            sm.CmdEnableAsync([this](state_machine::CommandResult result)
                {
                    enableResult = result;
                });
        }

        void ExpectRefreshStarted()
        {
            EXPECT_CALL(standstillMock, Await(_)).WillOnce(SaveArg<0>(&standstillDone));
        }

        void RotorComesToRest()
        {
            EXPECT_CALL(offsetsMock, Measure(_)).WillOnce(SaveArg<0>(&measurementDone));
            standstillDone(true);
        }

        void RefreshMeasures(const foc::PhaseCurrents& offsets)
        {
            RotorComesToRest();
            measurementDone(offsets);
        }

        StrictMock<services::IndexSearchMock> indexSearchMock;
        infra::Function<void(std::optional<foc::Radians>)> searchDone;
        services::IndexSearch::Config searchConfig;
        infra::Execute setupIndexSearchExpectations{ [this]()
            {
                EXPECT_CALL(indexSearchMock, Available()).WillRepeatedly(Return(true));
                EXPECT_CALL(indexSearchMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(indexSearchMock, Abort()).Times(AnyNumber());
            } };

        TestedStateMachine CreateStateMachineWithIndex()
        {
            return TestedStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ .electricalIdent = electricalIdentMock, .motorAlignment = alignmentMock, .indexSearch = indexSearchMock },
                faultNotifierMock,
                nvmActivity
            };
        }

        TestedStateMachine CreateStateMachineWithIndexRefreshingOffsets()
        {
            EXPECT_CALL(offsetsMock, IsRunning()).WillRepeatedly(Return(false));
            EXPECT_CALL(offsetsMock, Abort()).Times(AnyNumber());
            EXPECT_CALL(standstillMock, Abort()).Times(AnyNumber());

            return TestedStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ .electricalIdent = electricalIdentMock, .motorAlignment = alignmentMock, .currentOffsets = offsetsMock, .standstill = standstillMock, .indexSearch = indexSearchMock },
                faultNotifierMock,
                nvmActivity
            };
        }

        static services::CalibrationData RecordWithIndexAngle()
        {
            auto data = ValidRecord();
            data.encoderIndexAngle = 2.5f;
            return data;
        }

        void ExpectIndexSearch()
        {
            EXPECT_CALL(indexSearchMock, Search(_, _)).WillOnce(Invoke([this](const services::IndexSearch::Config& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone)
                {
                    searchConfig = config;
                    searchDone = onDone;
                }));
        }

        template<class Service>
        void ExpectAbortNow(Service& mock, const std::function<void()>& action)
        {
            EXPECT_CALL(mock, Abort());
            action();
            Mock::VerifyAndClearExpectations(&mock);
            EXPECT_CALL(mock, Abort()).Times(AnyNumber());
        }
    };
}

TEST_F(FocStateMachineTorqueTest, nvm_invalid_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, nvm_valid_on_boot_remains_in_idle_pending_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, enable_from_boot_with_valid_nvm_is_rejected_without_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, realign_from_idle_sets_rotor_reference_valid_in_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

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
    sm.CmdReAlign([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
}

TEST_F(FocStateMachineTorqueTest, enable_succeeds_after_realign)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, realign_alignment_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(std::nullopt);
            }));
    EXPECT_CALL(alignmentMock, LastFailure()).WillOnce(Return(services::MotorAlignment::Failure::none));
    sm.CmdReAlign([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(state_machine::FaultCode::calibrationFailed, std::get<state_machine::Fault>(sm.CurrentState()).code);
}

TEST_F(FocStateMachineTorqueTest, realign_that_finds_a_missing_phase_enters_fault_with_phase_missing)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(std::nullopt);
            }));
    EXPECT_CALL(alignmentMock, LastFailure()).WillOnce(Return(services::MotorAlignment::Failure::phaseMissing));
    sm.CmdReAlign([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(state_machine::FaultCode::phaseMissing, std::get<state_machine::Fault>(sm.CurrentState()).code);
    EXPECT_EQ(state_machine::FaultCode::phaseMissing, sm.LastFaultCode());
}

TEST_F(FocStateMachineTorqueTest, realign_nvm_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

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
                onDone(services::NvmStatus::WriteFailed);
            }));
    sm.CmdReAlign([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, realign_clears_stale_rotor_reference_before_starting)
{
    EXPECT_CALL(alignmentMock, LastFailure()).WillRepeatedly(Return(services::MotorAlignment::Failure::none));
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);
    ASSERT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);

    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(std::nullopt);
            }));
    sm.CmdReAlign([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, rotor_reference_preserved_after_disable_and_re_enable)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(2);
    sm.CmdEnable();
    sm.CmdDisable();
    ASSERT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::ok);
}

TEST_F(FocStateMachineTorqueTest, realign_from_idle_without_valid_calibration_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    auto result = state_machine::CommandResult::ok;
    sm.CmdReAlign([&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::rejected);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, nvm_load_failure_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([](services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::HardwareFault);
            }));
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, nvm_complete_stage_without_pole_pairs_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(0, 0.5f, services::CalibrationStage::complete);
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPartialCalibration());
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineTorqueTest, incomplete_record_without_pole_pairs_still_reports_partial_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(0, 0.5f, services::CalibrationStage::none);
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_TRUE(sm.HasPartialCalibration());
}

TEST_F(FocStateMachineTorqueTest, empty_record_on_boot_is_not_reported_as_partial_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(0, 0.0f, services::CalibrationStage::none);
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPartialCalibration());
}

TEST_F(FocStateMachineTorqueTest, nvm_incomplete_stage_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    services::CalibrationData incompleteData{};
    incompleteData.polePairs = 7;
    incompleteData.rPhase = 0.5f;
    incompleteData.lD = 1.0f;

    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([incompleteData](services::CalibrationData& out,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                out = incompleteData;
                onDone(services::NvmStatus::Ok);
            }));
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, accept_external_calibration_runs_alignment_before_entering_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    services::CalibrationData externalData{};
    externalData.polePairs = 4;
    externalData.rPhase = 0.5f;
    externalData.lD = 1.0f;

    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             infra::Function<void(std::optional<foc::Radians>)> cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_EQ(data.stage, services::CalibrationStage::complete);
                onDone(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    EXPECT_EQ(sm.CmdReserveExternalCalibration(), state_machine::CommandResult::ok);

    state_machine::CommandResult callbackResult = state_machine::CommandResult::rejected;
    sm.CmdCompleteExternalCalibration(externalData, [&callbackResult](state_machine::CommandResult r)
        {
            callbackResult = r;
        });

    EXPECT_EQ(callbackResult, state_machine::CommandResult::ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, external_calibration_carrying_a_non_finite_value_is_rejected_before_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    services::CalibrationData externalData{};
    externalData.polePairs = 4;
    externalData.rPhase = NonFiniteFloat(0x7F800000u);

    EXPECT_EQ(sm.CmdReserveExternalCalibration(), state_machine::CommandResult::ok);

    state_machine::CommandResult callbackResult = state_machine::CommandResult::ok;
    sm.CmdCompleteExternalCalibration(externalData, [&callbackResult](state_machine::CommandResult r)
        {
            callbackResult = r;
        });

    EXPECT_EQ(callbackResult, state_machine::CommandResult::rejected);
    EXPECT_FALSE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, a_persisted_record_with_a_non_finite_resistance_does_not_reach_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(7, NonFiniteFloat(0x7F800000u), services::CalibrationStage::complete);

    auto sm = CreateStateMachine();

    EXPECT_FALSE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, accept_external_calibration_cannot_enable_before_alignment_completes)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    services::CalibrationData externalData{};
    externalData.polePairs = 4;
    externalData.rPhase = 0.5f;

    infra::Function<void(std::optional<foc::Radians>)> capturedAlignCallback;
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([&capturedAlignCallback](std::size_t, const auto&,
                             infra::Function<void(std::optional<foc::Radians>)> cb)
            {
                capturedAlignCallback = cb;
            }));

    EXPECT_EQ(sm.CmdReserveExternalCalibration(), state_machine::CommandResult::ok);
    sm.CmdCompleteExternalCalibration(externalData, [](state_machine::CommandResult) {});

    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
    EXPECT_FALSE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, a_second_external_calibration_is_rejected_while_the_first_is_still_aligning)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    services::CalibrationData externalData{};
    externalData.polePairs = 4;
    externalData.rPhase = 0.5f;
    externalData.lD = 1.0f;

    infra::Function<void(std::optional<foc::Radians>)> capturedAlignCallback;
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([&capturedAlignCallback](std::size_t, const auto&,
                             infra::Function<void(std::optional<foc::Radians>)> cb)
            {
                capturedAlignCallback = cb;
            }));

    EXPECT_EQ(sm.CmdReserveExternalCalibration(), state_machine::CommandResult::ok);
    std::optional<state_machine::CommandResult> first;
    sm.CmdCompleteExternalCalibration(externalData, [&first](state_machine::CommandResult r)
        {
            first = r;
        });

    std::optional<state_machine::CommandResult> second;
    sm.CmdCompleteExternalCalibration(externalData, [&second](state_machine::CommandResult r)
        {
            second = r;
        });

    EXPECT_FALSE(first.has_value());
    EXPECT_EQ(state_machine::CommandResult::rejected, second);
    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, a_command_issued_from_a_completion_callback_is_completed_when_the_table_refuses_it)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));

    std::optional<state_machine::CommandResult> reAlignResult;
    sm.CmdClearCalibration([&sm, &reAlignResult](state_machine::CommandResult)
        {
            sm.CmdReAlign([&reAlignResult](state_machine::CommandResult r)
                {
                    reAlignResult = r;
                });
        });

    EXPECT_EQ(state_machine::CommandResult::rejected, reAlignResult);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, async_work_stays_pending_while_a_save_is_outstanding_after_an_emergency_stop)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    infra::Function<void(services::NvmStatus)> capturedSave;
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&capturedSave](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedSave = onDone;
            }));

    std::optional<state_machine::CommandResult> result;
    sm.CmdCalibrate([&result](state_machine::CommandResult r)
        {
            result = r;
        });
    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdEmergencyStop();
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, result);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_TRUE(sm.HasPendingAsyncWork());

    capturedSave(services::NvmStatus::Ok);

    EXPECT_FALSE(sm.HasPendingAsyncWork());
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, a_busy_answer_to_the_save_of_an_aborted_run_is_not_retried)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    infra::Function<void(services::NvmStatus)> capturedSave;
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&capturedSave](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedSave = onDone;
            }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});
    sm.CmdEmergencyStop();

    capturedSave(services::NvmStatus::Busy);
    ForwardTime(std::chrono::seconds{ 1 });

    EXPECT_FALSE(sm.HasPendingAsyncWork());
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, calibrate_is_rejected_while_the_save_of_an_aborted_run_is_still_outstanding)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    infra::Function<void(services::NvmStatus)> capturedSave;
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&capturedSave](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedSave = onDone;
            }));

    sm.CmdCalibrate([](state_machine::CommandResult) {});
    sm.CmdEmergencyStop();
    ASSERT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));

    std::optional<state_machine::CommandResult> result;
    sm.CmdCalibrate([&result](state_machine::CommandResult value)
        {
            result = value;
        });
    EXPECT_EQ(state_machine::CommandResult::rejected, result);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));

    capturedSave(services::NvmStatus::Ok);
    EXPECT_FALSE(sm.HasPendingAsyncWork());

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, set_flux_linkage_is_rejected_while_the_save_of_an_aborted_command_is_still_outstanding)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    infra::Function<void(services::NvmStatus)> capturedSave;
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&capturedSave](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedSave = onDone;
            }));
    std::optional<state_machine::CommandResult> first;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, 0.05f, [&first](state_machine::CommandResult value)
        {
            first = value;
        });
    sm.CmdEmergencyStop();
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, first);

    std::optional<state_machine::CommandResult> second;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, 0.06f, [&second](state_machine::CommandResult value)
        {
            second = value;
        });
    EXPECT_EQ(state_machine::CommandResult::rejected, second);
    EXPECT_TRUE(sm.HasPendingAsyncWork());

    capturedSave(services::NvmStatus::Ok);
    EXPECT_FALSE(sm.HasPendingAsyncWork());
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, calibrate_from_idle_runs_full_sequence_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, calibrate_from_ready_re_runs_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, calibrate_records_pole_pairs_in_ready_state)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    const auto& ready = std::get<state_machine::Ready>(sm.CurrentState());
    EXPECT_EQ(ready.loadedData.polePairs, uint8_t{ 7 });
}

TEST_F(FocStateMachineTorqueTest, calibrate_records_resistance_in_ready_state)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    const auto& ready = std::get<state_machine::Ready>(sm.CurrentState());
    EXPECT_NEAR(ready.loadedData.rPhase, 0.5f, 1e-5f);
}

TEST_F(FocStateMachineTorqueTest, calibrate_identifies_the_flux_linkage_after_alignment_and_records_it)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    ExpectFluxLinkageIdentification(services::FluxLinkageIdentification::Outcome::identified, 0.0046f);
    auto sm = CreateStateMachineIdentifyingFluxLinkage();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    const auto& ready = std::get<state_machine::Ready>(sm.CurrentState());
    EXPECT_NEAR(ready.loadedData.fluxLinkage, 0.0046f, 1e-7f);
}

TEST_F(FocStateMachineTorqueTest, a_flux_linkage_that_is_not_identified_keeps_the_configured_one)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    ExpectFluxLinkageIdentification(services::FluxLinkageIdentification::Outcome::notIdentified, 0.0f);
    auto sm = CreateStateMachineIdentifyingFluxLinkage();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    const auto& ready = std::get<state_machine::Ready>(sm.CurrentState());
    EXPECT_NEAR(ready.loadedData.fluxLinkage, configuredFluxLinkage.Value(), 1e-7f);
}

TEST_F(FocStateMachineTorqueTest, an_overspeed_during_flux_linkage_identification_fails_the_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    ExpectFluxLinkageIdentification(services::FluxLinkageIdentification::Outcome::overspeed, 0.0f);
    auto sm = CreateStateMachineIdentifyingFluxLinkage();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, realign_does_not_identify_the_flux_linkage)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachineIdentifyingFluxLinkage();

    AlignAfterBoot(sm);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, calibrate_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code,
        state_machine::FaultCode::calibrationFailed);
}

TEST_F(FocStateMachineTorqueTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, alignment_nullopt_enters_fault)
{
    EXPECT_CALL(alignmentMock, LastFailure()).WillRepeatedly(Return(services::MotorAlignment::Failure::none));
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, nvm_save_failure_after_full_calibration_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, nvm_save_busy_after_full_calibration_is_retried_and_reaches_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Busy);
            }))
        .WillOnce(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
    EXPECT_TRUE(sm.HasPendingAsyncWork());

    ForwardTime(std::chrono::milliseconds{ 10 });

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, emergency_stop_while_a_busy_calibration_save_waits_cancels_the_retry)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Busy);
            }));
    EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());

    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
    EXPECT_TRUE(sm.HasPendingAsyncWork());

    sm.CmdEmergencyStop();
    ForwardTime(std::chrono::milliseconds{ 50 });

    EXPECT_FALSE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, enable_from_ready_starts_foc_and_enters_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, disable_from_enabled_stops_foc_and_enters_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, disable_from_ready_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, emergency_stop_from_enabled_with_valid_calibration_returns_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(2);
    sm.CmdEnable();

    EXPECT_EQ(sm.CmdEmergencyStop(), state_machine::CommandResult::ok);

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Ready>(sm.CurrentState()).loadedData.polePairs, 7);
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, emergency_stop_during_calibration_without_valid_calibration_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));

    auto commandResult = state_machine::CommandResult::ok;
    sm.CmdCalibrate([&commandResult](state_machine::CommandResult result)
        {
            commandResult = result;
        });
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    EXPECT_EQ(sm.CmdEmergencyStop(), state_machine::CommandResult::ok);

    EXPECT_EQ(commandResult, state_machine::CommandResult::abortedByFault);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, emergency_stop_during_calibration_keeps_previously_valid_calibration)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));

    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    EXPECT_EQ(sm.CmdEmergencyStop(), state_machine::CommandResult::ok);

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Ready>(sm.CurrentState()).loadedData.polePairs, 7);
}

TEST_F(FocStateMachineTorqueTest, emergency_stop_from_idle_with_loaded_calibration_keeps_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_EQ(sm.CmdEmergencyStop(), state_machine::CommandResult::ok);

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, fault_from_enabled_stops_pwm_and_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, fault_from_idle_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overvoltage);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, fault_from_idle_with_loaded_calibration_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::encoderLoss);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, fault_code_is_recorded)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(std::get<state_machine::Fault>(sm.CurrentState()).code,
        state_machine::FaultCode::overcurrent);
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachineTorqueTest, clear_fault_from_fault_without_calibration_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_fault_from_fault_with_valid_calibration_returns_to_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, a_fault_raised_while_starting_leaves_the_drive_in_fault_rather_than_enabled)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start())
        .WillOnce(Invoke([this]()
            {
                faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
            }));

    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::abortedByFault);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, a_latched_fault_refuses_a_new_enable_until_it_is_cleared)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start())
        .WillOnce(Invoke([this]()
            {
                faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
            }));

    sm.CmdEnable();

    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineTorqueTest, a_condition_that_keeps_re_faulting_is_refused_after_the_retry_limit)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());

    for (int attempt = 0; attempt != 3; ++attempt)
    {
        faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
        EXPECT_EQ(sm.CmdClearFault(), state_machine::CommandResult::ok);
    }

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_EQ(sm.CmdClearFault(), state_machine::CommandResult::rejected);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, a_clean_enable_and_disable_cycle_resets_the_fault_retry_budget)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());

    for (int attempt = 0; attempt != 3; ++attempt)
    {
        faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
        EXPECT_EQ(sm.CmdClearFault(), state_machine::CommandResult::ok);
    }

    sm.CmdEnable();
    sm.CmdDisable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_EQ(sm.CmdClearFault(), state_machine::CommandResult::ok);
}

TEST_F(FocStateMachineTorqueTest, a_clean_run_while_enabled_resets_the_fault_retry_budget)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());

    for (int attempt = 0; attempt != 3; ++attempt)
    {
        faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
        EXPECT_EQ(sm.CmdClearFault(), state_machine::CommandResult::ok);
    }

    sm.CmdEnable();
    ForwardTime(application::FaultController::cleanRunDuration);
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_EQ(sm.CmdClearFault(), state_machine::CommandResult::ok);
}

TEST_F(FocStateMachineTorqueTest, a_run_shorter_than_the_clean_run_keeps_the_fault_retry_budget_spent)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());

    for (int attempt = 0; attempt != 3; ++attempt)
    {
        faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
        EXPECT_EQ(sm.CmdClearFault(), state_machine::CommandResult::ok);
    }

    sm.CmdEnable();
    ForwardTime(application::FaultController::cleanRunDuration - std::chrono::seconds(1));
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ForwardTime(std::chrono::seconds(2));

    EXPECT_EQ(sm.CmdClearFault(), state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineTorqueTest, clear_fault_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_cal_from_ready_invalidates_nvm_and_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_cal_from_idle_calls_invalidate)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_cal_while_nvm_is_busy_completes_with_rejected_and_keeps_the_record)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Busy);
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });

    EXPECT_EQ(state_machine::CommandResult::rejected, result);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, fault_during_calibration_stops_inverter_and_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    EXPECT_CALL(inverterMock, Stop()).Times(AtLeast(1));
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, late_pole_pairs_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<std::size_t>)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([&capturedCb](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(std::size_t{ 7 });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, late_resistance_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([&capturedCb](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, late_alignment_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<foc::Radians>)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([&capturedCb](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(foc::Radians{ 0.0f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, late_nvm_save_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&capturedCb](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, calibrate_is_rejected_until_the_boot_check_has_completed)
{
    GivenFaultNotifierRegistered();
    infra::Function<void(services::NvmStatus)> bootCb;
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([&bootCb](services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                bootCb = onDone;
            }));
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    std::optional<state_machine::CommandResult> result;
    sm.CmdCalibrate([&result](state_machine::CommandResult value)
        {
            result = value;
        });
    EXPECT_EQ(state_machine::CommandResult::rejected, result);
    ASSERT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));

    bootCb(services::NvmStatus::InvalidData);
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_cal_during_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_cal_nvm_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    ASSERT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::WriteFailed);
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

namespace
{
    class FocStateMachineTorqueAutoTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        using AutoStateMachine = application::TorqueStateMachine;

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
        StrictMock<services::NonVolatileMemoryMock> nvmMock;
        application::NvmActivity nvmActivity;
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdentMock;
        StrictMock<services::MotorAlignmentMock> alignmentMock;
        StrictMock<state_machine::FaultNotifierMock> faultNotifierMock;
        infra::Execute setupTeardownExpectations{ [this]()
            {
                EXPECT_CALL(electricalIdentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(electricalIdentMock, IsRunning()).WillRepeatedly(Return(false));
                EXPECT_CALL(alignmentMock, Abort()).Times(AnyNumber());
                EXPECT_CALL(faultNotifierMock, Unregister()).Times(AnyNumber());
            } };

        foc::Volts vdc{ 24.0f };

        infra::Execute setupInverterExpectations{ [this]()
            {
                EXPECT_CALL(inverterMock, BaseFrequency())
                    .Times(AnyNumber())
                    .WillRepeatedly(Return(hal::Hertz{ 10000 }));
                EXPECT_CALL(inverterMock, PhaseCurrentsReady(_, _)).Times(AnyNumber());
                EXPECT_CALL(inverterMock, Stop()).Times(AnyNumber());
            } };

        void GivenFaultNotifierRegistered()
        {
            EXPECT_CALL(faultNotifierMock, Register(_, _))
                .WillOnce(Invoke([this](const infra::Function<void(state_machine::FaultCode)>& immediate, const infra::Function<void(state_machine::FaultCode)>& deferred)
                    {
                        faultNotifierMock.StoreHandler(immediate, deferred);
                    }));
        }

        void GivenNvmInvalid()
        {
            EXPECT_CALL(nvmMock, LoadCalibration(_, _))
                .WillOnce(Invoke([](services::CalibrationData& out, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        out = services::CalibrationData{};
                        onDone(services::NvmStatus::InvalidData);
                    }));
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

        void ExpectCalibrationSequence(bool polePairsOk = true,
            bool resistanceOk = true,
            bool alignmentOk = true,
            bool nvmSaveOk = true)
        {
            if (polePairsOk)
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::size_t{ 7 });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
                    .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
                        {
                            cb(std::nullopt);
                        }));
                return;
            }

            if (resistanceOk)
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                        {
                            cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
                        }));
            else
            {
                EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
                    .WillOnce(Invoke([](const auto&,
                                         const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                        {
                            cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{});
                        }));
                return;
            }

            if (alignmentOk)
                EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                    .WillOnce(Invoke([](std::size_t, const auto&,
                                         const infra::Function<void(std::optional<foc::Radians>)>& cb)
                        {
                            cb(foc::Radians{ 0.0f });
                        }));
            else
            {
                EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
                    .WillOnce(Invoke([](std::size_t, const auto&,
                                         const infra::Function<void(std::optional<foc::Radians>)>& cb)
                        {
                            cb(std::nullopt);
                        }));
                return;
            }

            if (nvmSaveOk)
            {
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::Ok);
                        }));
                EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
            }
            else
                EXPECT_CALL(nvmMock, SaveCalibration(_, _))
                    .WillOnce(Invoke([](const services::CalibrationData&,
                                         infra::Function<void(services::NvmStatus)> onDone)
                        {
                            onDone(services::NvmStatus::WriteFailed);
                        }));
        }

        AutoStateMachine CreateStateMachine()
        {
            return AutoStateMachine{
                tracer,
                application::MotorHardware{ inverterMock, encoderMock, vdc },
                nvmMock,
                application::CalibrationServices{ electricalIdentMock, alignmentMock },
                faultNotifierMock,
                nvmActivity
            };
        }

        void AlignAfterBoot(AutoStateMachine& sm)
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
            EXPECT_CALL(encoderMock, Set(_)).Times(AnyNumber());
            sm.CmdReAlign([](state_machine::CommandResult) {});
        }
    };
}

TEST_F(FocStateMachineTorqueAutoTest, starts_in_idle_when_nvm_invalid)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, nvm_valid_on_boot_remains_in_idle_pending_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, nvm_load_failure_on_boot_remains_in_idle)
{
    GivenFaultNotifierRegistered();
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([](services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::HardwareFault);
            }));
    auto sm = CreateStateMachine();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, calibrate_enable_disable_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdDisable();
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, calibrate_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, pole_pairs_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, resistance_inductance_nullopt_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, alignment_nullopt_enters_fault)
{
    EXPECT_CALL(alignmentMock, LastFailure()).WillRepeatedly(Return(services::MotorAlignment::Failure::none));
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, nvm_save_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence(true, true, true, false);
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, enable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, disable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, fault_from_enabled_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachineTorqueAutoTest, fault_and_clear_cycle)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdClearFault();
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_fault_from_non_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_cal_from_ready_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_cal_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, late_pole_pairs_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<std::size_t>)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([&capturedCb](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(std::size_t{ 7 });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_cal_during_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_cal_nvm_failure_enters_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::WriteFailed);
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, enable_is_rejected_while_a_clear_calibration_is_outstanding)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_EQ(state_machine::CommandResult::rejected, sm.CmdEnable());
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    capturedCb(services::NvmStatus::Ok);
    EXPECT_EQ(state_machine::CommandResult::ok, result);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_cal_invalidated_after_a_fault_drops_the_record_so_clear_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, result);

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPendingAsyncWork());

    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdClearFault());
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_cal_invalidate_failure_callback_after_fault_does_not_re_enter_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    ASSERT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);

    capturedCb(services::NvmStatus::WriteFailed);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachineTorqueAutoTest, enable_is_rejected_while_a_clear_calibration_is_outstanding)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_EQ(state_machine::CommandResult::rejected, sm.CmdEnable());
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    capturedCb(services::NvmStatus::Ok);
    EXPECT_EQ(state_machine::CommandResult::ok, result);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_cal_invalidated_after_a_fault_drops_the_record_so_clear_fault_returns_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, result);

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPendingAsyncWork());

    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdClearFault());
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_cal_invalidate_failure_callback_after_fault_does_not_re_enter_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    ASSERT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);

    capturedCb(services::NvmStatus::WriteFailed);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(sm.LastFaultCode(), state_machine::FaultCode::overcurrent);
}

TEST_F(FocStateMachineTorqueAutoTest, late_resistance_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([&capturedCb](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, late_alignment_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(std::optional<foc::Radians>)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([&capturedCb](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                capturedCb = cb;
            }));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(foc::Radians{ 0.0f });
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, late_nvm_save_callback_after_fault_is_ignored)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                cb(std::size_t{ 7 });
            }));
    EXPECT_CALL(electricalIdentMock, EstimateResistanceAndInductance(_, _))
        .WillOnce(Invoke([](const auto&,
                             const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
            {
                cb(services::ElectricalParametersIdentification::ResistanceInductanceResult{ foc::Ohm{ 0.5f }, foc::MilliHenry{ 1.0f }, 1.0f });
            }));
    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&,
                             const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&capturedCb](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, calibrate_is_rejected_until_the_boot_check_has_completed)
{
    GivenFaultNotifierRegistered();
    infra::Function<void(services::NvmStatus)> bootCb;
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([&bootCb](services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                bootCb = onDone;
            }));
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    std::optional<state_machine::CommandResult> result;
    sm.CmdCalibrate([&result](state_machine::CommandResult value)
        {
            result = value;
        });
    EXPECT_EQ(state_machine::CommandResult::rejected, result);
    ASSERT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));

    bootCb(services::NvmStatus::InvalidData);
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, calibrate_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, calibrate_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, calibrate_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, calibrate_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, enable_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, enable_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, enable_from_enabled_does_not_call_start_again)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, enable_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, enable_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, enable_from_enabled_does_not_call_start_again)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    sm.CmdEnable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, disable_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, disable_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, disable_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, disable_from_nvm_invalid_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, disable_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, disable_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdDisable();

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_fault_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_fault_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_fault_from_idle_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_fault_from_calibrating_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([](const auto&, const infra::Function<void(std::optional<std::size_t>)>&) {}));
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_fault_from_ready_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    sm.CmdClearFault();

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, clear_cal_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueAutoTest, clear_cal_from_fault_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));

    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, set_flux_linkage_in_ready_persists_and_returns_ok)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));

    auto result = state_machine::CommandResult::rejected;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, 0.05f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, set_flux_linkage_to_zero_returns_to_the_configured_default)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    const auto configured = sm.ActiveFluxLinkage();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(data.fluxLinkage, 0.0f, 1e-9f);
                onDone(services::NvmStatus::Ok);
            }));

    auto result = state_machine::CommandResult::rejected;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, 0.0f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::ok);
    EXPECT_NEAR(sm.ActiveFluxLinkage().Value(), configured.Value(), 1e-6f);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, changing_the_pole_pairs_in_ready_requires_another_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_EQ(data.polePairs, 5);
                onDone(services::NvmStatus::Ok);
            }));

    auto result = state_machine::CommandResult::rejected;
    sm.CmdSetMotorParameter(services::MotorParameter::polePairs, 5.0f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::ok);
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_FALSE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
    EXPECT_EQ(std::get<state_machine::Ready>(sm.CurrentState()).loadedData.polePairs, 5);
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::rejected);

    AlignAfterBoot(sm);
    EXPECT_CALL(inverterMock, Start()).Times(1);
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::ok);
}

TEST_F(FocStateMachineTorqueTest, changing_the_pole_pairs_drops_the_cogging_map_and_the_index_angle)
{
    GivenFaultNotifierRegistered();
    auto record = ValidRecord();
    record.coggingTorqueScale = 0.01f;
    record.coggingTorque.fill(10);
    record.encoderIndexAngle = 1.5f;
    GivenNvmHolds(record);
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(data.coggingTorqueScale, 0.0f, 1e-9f);
                EXPECT_EQ(data.coggingTorque, (std::array<int8_t, services::coggingMapBins>{}));
                EXPECT_FALSE(services::EncoderIndexAngle(data).has_value());
                onDone(services::NvmStatus::Ok);
            }));

    sm.CmdSetMotorParameter(services::MotorParameter::polePairs, 5.0f, [](state_machine::CommandResult) {});
}

TEST_F(FocStateMachineTorqueTest, changing_the_resistance_keeps_the_cogging_map)
{
    GivenFaultNotifierRegistered();
    auto record = ValidRecord();
    record.coggingTorqueScale = 0.01f;
    record.coggingTorque.fill(10);
    GivenNvmHolds(record);
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(data.coggingTorqueScale, 0.01f, 1e-9f);
                EXPECT_EQ(data.coggingTorque[0], 10);
                onDone(services::NvmStatus::Ok);
            }));

    sm.CmdSetMotorParameter(services::MotorParameter::resistance, 0.36f, [](state_machine::CommandResult) {});
}

TEST_F(FocStateMachineTorqueTest, changing_the_resistance_in_ready_keeps_the_rotor_reference)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(data.rPhase, 0.36f, 1e-6f);
                EXPECT_EQ(data.stage, services::CalibrationStage::complete);
                onDone(services::NvmStatus::Ok);
            }));

    sm.CmdSetMotorParameter(services::MotorParameter::resistance, 0.36f, [](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_NEAR(std::get<state_machine::Ready>(sm.CurrentState()).loadedData.rPhase, 0.36f, 1e-6f);
    EXPECT_CALL(inverterMock, Start()).Times(1);
    EXPECT_EQ(sm.CmdEnable(), state_machine::CommandResult::ok);
}

TEST_F(FocStateMachineTorqueTest, an_inductance_is_written_to_both_axes)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(data.lD, 0.2f, 1e-6f);
                onDone(services::NvmStatus::Ok);
            }));

    sm.CmdSetMotorParameter(services::MotorParameter::inductance, 0.2f, [](state_machine::CommandResult) {});

    EXPECT_NEAR(sm.CalibrationRecord().lD, 0.2f, 1e-6f);
}

TEST_F(FocStateMachineTorqueTest, a_datasheet_record_entered_from_scratch_becomes_complete_and_can_be_aligned)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .Times(3)
        .WillRepeatedly(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));

    sm.CmdSetMotorParameter(services::MotorParameter::resistance, 0.36f, [](state_machine::CommandResult) {});
    EXPECT_TRUE(sm.HasPartialCalibration());
    sm.CmdSetMotorParameter(services::MotorParameter::inductance, 0.2f, [](state_machine::CommandResult) {});
    sm.CmdSetMotorParameter(services::MotorParameter::polePairs, 4.0f, [](state_machine::CommandResult) {});

    EXPECT_EQ(sm.CalibrationRecord().stage, services::CalibrationStage::complete);
    EXPECT_FALSE(sm.HasPartialCalibration());
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));

    AlignAfterBoot(sm);
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, set_flux_linkage_with_negative_value_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    auto result = state_machine::CommandResult::ok;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, -0.05f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineTorqueTest, set_flux_linkage_without_a_calibration_is_stored_for_the_next_one)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                EXPECT_NEAR(data.fluxLinkage, 0.05f, 1e-6f);
                EXPECT_EQ(data.stage, services::CalibrationStage::none);
                onDone(services::NvmStatus::Ok);
            }));

    auto result = state_machine::CommandResult::rejected;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, 0.05f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::ok);
    EXPECT_NEAR(sm.ActiveFluxLinkage().Value(), 0.05f, 1e-6f);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPartialCalibration());
}

TEST_F(FocStateMachineTorqueTest, set_flux_linkage_from_enabled_is_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start()).Times(1);
    sm.CmdEnable();
    ASSERT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));

    auto result = state_machine::CommandResult::ok;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, 0.05f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::rejected);
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, set_flux_linkage_nvm_busy_returns_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Busy);
            }));

    auto result = state_machine::CommandResult::ok;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, 0.05f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::rejected);
}

TEST_F(FocStateMachineTorqueTest, set_flux_linkage_nvm_write_failed_returns_nvmFailed)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::WriteFailed);
            }));

    auto result = state_machine::CommandResult::ok;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, 0.05f, [&result](state_machine::CommandResult r)
        {
            result = r;
        });

    EXPECT_EQ(result, state_machine::CommandResult::nvmFailed);
}

TEST_F(FocStateMachineTorqueTest, has_pending_async_work_true_during_nvm_boot_check)
{
    GivenFaultNotifierRegistered();
    infra::Function<void(services::NvmStatus)> bootCb;
    EXPECT_CALL(nvmMock, LoadCalibration(_, _))
        .WillOnce(Invoke([&bootCb](services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                bootCb = onDone;
            }));
    auto sm = CreateStateMachine();

    EXPECT_TRUE(sm.HasPendingAsyncWork());

    bootCb(services::NvmStatus::InvalidData);
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, has_pending_async_work_false_after_nvm_loaded)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, has_pending_async_work_true_when_electrical_ident_is_running)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_CALL(electricalIdentMock, IsRunning()).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_TRUE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, register_ready_handler_called_when_state_enters_ready)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    bool handlerCalled = false;
    sm.RegisterReadyHandler([&handlerCalled]()
        {
            handlerCalled = true;
        });

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    EXPECT_TRUE(handlerCalled);
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, flux_linkage_stored_after_a_fault_is_still_applied_to_the_record)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([&capturedCb](const services::CalibrationData&,
                             infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));

    std::optional<state_machine::CommandResult> result;
    sm.CmdSetMotorParameter(services::MotorParameter::fluxLinkage, 0.05f, [&result](state_machine::CommandResult value)
        {
            result = value;
        });

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, result);

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_NEAR(0.05f, sm.ActiveFluxLinkage().Value(), 1e-6f);
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, clear_cal_invalidated_after_the_fault_was_cleared_returns_the_machine_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    sm.CmdClearCalibration([](state_machine::CommandResult) {});

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdClearFault());
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::rejected, sm.CmdEnable());

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, clear_cal_invalidated_after_an_emergency_stop_drops_the_record)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    infra::Function<void(services::NvmStatus)> capturedCb;
    EXPECT_CALL(nvmMock, InvalidateCalibration(_))
        .WillOnce(Invoke([&capturedCb](infra::Function<void(services::NvmStatus)> onDone)
            {
                capturedCb = onDone;
            }));
    std::optional<state_machine::CommandResult> result;
    sm.CmdClearCalibration([&result](state_machine::CommandResult value)
        {
            result = value;
        });

    sm.CmdEmergencyStop();
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, result);
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    capturedCb(services::NvmStatus::Ok);
    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

namespace
{
    TEST_F(FocStateMachineTorqueTest, a_fault_during_calibration_aborts_the_identification_services)
    {
        GivenFaultNotifierRegistered();
        GivenNvmInvalid();

        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _));

        auto sm = CreateStateMachine();
        sm.CmdCalibrate([](state_machine::CommandResult) {});
        ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

        EXPECT_CALL(electricalIdentMock, Abort()).Times(AtLeast(1)).RetiresOnSaturation();
        EXPECT_CALL(alignmentMock, Abort()).Times(AtLeast(1)).RetiresOnSaturation();

        faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

        ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    }

    TEST_F(FocStateMachineTorqueTest, an_emergency_stop_during_calibration_aborts_the_identification_services)
    {
        GivenFaultNotifierRegistered();
        GivenNvmInvalid();

        EXPECT_CALL(electricalIdentMock, EstimateNumberOfPolePairs(_, _));

        auto sm = CreateStateMachine();
        sm.CmdCalibrate([](state_machine::CommandResult) {});
        ASSERT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));

        EXPECT_CALL(electricalIdentMock, Abort()).Times(AtLeast(1)).RetiresOnSaturation();
        EXPECT_CALL(alignmentMock, Abort()).Times(AtLeast(1)).RetiresOnSaturation();

        sm.CmdEmergencyStop();

        EXPECT_FALSE(sm.HasPendingAsyncWork());
    }

    TEST_F(FocStateMachineTorqueTest, a_further_fault_while_faulted_keeps_the_first_fault_code)
    {
        GivenFaultNotifierRegistered();
        GivenNvmInvalid();
        auto sm = CreateStateMachine();

        faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
        faultNotifierMock.TriggerFault(state_machine::FaultCode::overvoltage);

        ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
        EXPECT_EQ(state_machine::FaultCode::overcurrent, std::get<state_machine::Fault>(sm.CurrentState()).code);
        EXPECT_EQ(state_machine::FaultCode::overcurrent, sm.LastFaultCode());
    }

    TEST_F(FocStateMachineTorqueTest, the_first_fault_code_survives_a_clear_and_a_new_fault_records_the_new_one)
    {
        GivenFaultNotifierRegistered();
        GivenNvmInvalid();
        auto sm = CreateStateMachine();

        faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
        faultNotifierMock.TriggerFault(state_machine::FaultCode::overvoltage);
        EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdClearFault());

        faultNotifierMock.TriggerFault(state_machine::FaultCode::overtemperature);

        ASSERT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
        EXPECT_EQ(state_machine::FaultCode::overtemperature, sm.LastFaultCode());
    }

    TEST_F(FocStateMachineTorqueTest, a_synchronous_command_issued_from_a_completion_callback_reports_queued_not_ok)
    {
        GivenFaultNotifierRegistered();
        GivenNvmValid();
        auto sm = CreateStateMachine();

        EXPECT_CALL(nvmMock, InvalidateCalibration(_))
            .WillOnce(Invoke([](infra::Function<void(services::NvmStatus)> onDone)
                {
                    onDone(services::NvmStatus::Ok);
                }));

        std::optional<state_machine::CommandResult> enableResult;
        sm.CmdClearCalibration([&sm, &enableResult](state_machine::CommandResult)
            {
                enableResult = sm.CmdEnable();
            });

        EXPECT_EQ(state_machine::CommandResult::queued, enableResult);
        EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
    }

    TEST_F(FocStateMachineTorqueTest, a_second_external_completion_without_a_callback_is_still_refused_while_aligning)
    {
        GivenFaultNotifierRegistered();
        GivenNvmInvalid();
        auto sm = CreateStateMachine();

        services::CalibrationData externalData{};
        externalData.polePairs = 7;
        externalData.rPhase = 0.5f;
        externalData.lD = 1.0f;

        EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
            .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>&) {}));

        EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdReserveExternalCalibration());
        sm.CmdCompleteExternalCalibration(externalData, infra::Function<void(state_machine::CommandResult)>{});

        std::optional<state_machine::CommandResult> second;
        sm.CmdCompleteExternalCalibration(externalData, [&second](state_machine::CommandResult r)
            {
                second = r;
            });

        EXPECT_EQ(state_machine::CommandResult::rejected, second);
        EXPECT_TRUE(std::holds_alternative<state_machine::Calibrating>(sm.CurrentState()));
    }

    TEST_F(FocStateMachineTorqueTest, destruction_releases_the_fault_registration_and_the_services)
    {
        GivenFaultNotifierRegistered();
        GivenNvmValid();

        {
            auto sm = CreateStateMachine();

            EXPECT_CALL(electricalIdentMock, Abort()).Times(AtLeast(1)).RetiresOnSaturation();
            EXPECT_CALL(alignmentMock, Abort()).Times(AtLeast(1)).RetiresOnSaturation();
            EXPECT_CALL(faultNotifierMock, Unregister())
                .Times(AtLeast(1))
                .WillRepeatedly(Invoke([this]()
                    {
                        faultNotifierMock.ReleaseHandler();
                    }))
                .RetiresOnSaturation();
        }

        faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    }
}

TEST_F(FocStateMachineTorqueTest, the_first_enable_measures_the_offsets_at_rest_while_ready)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);

    ExpectRefreshStarted();
    EnableAsync(sm);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_TRUE(sm.HasPendingAsyncWork());
    EXPECT_FALSE(enableResult.has_value());
}

TEST_F(FocStateMachineTorqueTest, offsets_within_50_mA_of_the_record_are_applied_and_the_drive_enables)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);
    ExpectRefreshStarted();
    EnableAsync(sm);

    EXPECT_CALL(offsetsMock, Apply(OffsetsNear(0.055f, -0.020f, 0.030f)));
    EXPECT_CALL(inverterMock, Start());
    RefreshMeasures(foc::PhaseCurrents{ foc::Ampere{ 0.055f }, foc::Ampere{ -0.020f }, foc::Ampere{ 0.030f } });

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::ok, enableResult);
}

TEST_F(FocStateMachineTorqueTest, an_offset_more_than_50_mA_from_the_record_refuses_the_enable)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);
    ExpectRefreshStarted();
    EnableAsync(sm);

    RefreshMeasures(foc::PhaseCurrents{ foc::Ampere{ 0.010f }, foc::Ampere{ -0.075f }, foc::Ampere{ 0.030f } });

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::calibrationFailed, enableResult);
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, a_rotor_that_does_not_come_to_rest_refuses_the_enable)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);
    EXPECT_CALL(standstillMock, Await(_)).WillOnce(SaveArg<0>(&standstillDone));
    EnableAsync(sm);

    standstillDone(false);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::calibrationFailed, enableResult);
}

TEST_F(FocStateMachineTorqueTest, a_failed_offset_measurement_refuses_the_enable)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);
    ExpectRefreshStarted();
    EnableAsync(sm);

    RotorComesToRest();
    measurementDone(std::nullopt);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::calibrationFailed, enableResult);
}

TEST_F(FocStateMachineTorqueTest, a_later_enable_does_not_measure_the_offsets_again)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);
    ExpectRefreshStarted();
    EnableAsync(sm);
    EXPECT_CALL(offsetsMock, Apply(OffsetsNear(0.012f, -0.020f, 0.030f)));
    EXPECT_CALL(inverterMock, Start()).Times(2);
    RefreshMeasures(foc::PhaseCurrents{ foc::Ampere{ 0.012f }, foc::Ampere{ -0.020f }, foc::Ampere{ 0.030f } });
    sm.CmdDisable();

    enableResult.reset();
    EnableAsync(sm);

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::ok, enableResult);
}

TEST_F(FocStateMachineTorqueTest, a_fault_makes_the_next_enable_measure_the_offsets_again)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);
    ExpectRefreshStarted();
    EnableAsync(sm);
    EXPECT_CALL(offsetsMock, Apply(OffsetsNear(0.012f, -0.020f, 0.030f)));
    EXPECT_CALL(inverterMock, Start());
    RefreshMeasures(foc::PhaseCurrents{ foc::Ampere{ 0.012f }, foc::Ampere{ -0.020f }, foc::Ampere{ 0.030f } });

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
    ASSERT_EQ(state_machine::CommandResult::ok, sm.CmdClearFault());

    enableResult.reset();
    ExpectRefreshStarted();
    EnableAsync(sm);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_FALSE(enableResult.has_value());
}

TEST_F(FocStateMachineTorqueTest, a_disable_during_the_offset_refresh_abandons_it_and_refuses_the_enable)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);
    ExpectRefreshStarted();
    EnableAsync(sm);
    RotorComesToRest();

    ExpectAbortNow(offsetsMock, [&sm]()
        {
            EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdDisable());
        });
    EXPECT_CALL(offsetsMock, IsRunning()).WillRepeatedly(Return(false));

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::rejected, enableResult);
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, an_emergency_stop_during_the_offset_refresh_abandons_it)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);
    ExpectRefreshStarted();
    EnableAsync(sm);

    sm.CmdEmergencyStop();
    standstillDone(true);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, enableResult);
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, the_synchronous_enable_is_accepted_while_the_offsets_are_refreshed)
{
    GivenStoredOffsets();
    auto sm = CreateStateMachineRefreshingOffsets();
    AlignAfterBoot(sm);
    ExpectRefreshStarted();

    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdEnable());

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_CALL(offsetsMock, Apply(OffsetsNear(0.010f, -0.020f, 0.030f)));
    EXPECT_CALL(inverterMock, Start());
    RefreshMeasures(foc::PhaseCurrents{ foc::Ampere{ 0.010f }, foc::Ampere{ -0.020f }, foc::Ampere{ 0.030f } });
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, an_enable_refused_without_a_callback_reports_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EXPECT_EQ(state_machine::CommandResult::rejected, sm.CmdEnable());
}

TEST_F(FocStateMachineTorqueTest, an_asynchronous_enable_refused_in_idle_reports_rejected)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();

    EnableAsync(sm);

    EXPECT_EQ(state_machine::CommandResult::rejected, enableResult);
}

TEST_F(FocStateMachineTorqueTest, an_asynchronous_enable_without_an_offset_measurement_enables_at_once)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start());
    EnableAsync(sm);

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::ok, enableResult);
}

TEST_F(FocStateMachineTorqueTest, an_asynchronous_enable_that_faults_as_the_bridge_starts_reports_aborted_by_fault)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    EXPECT_CALL(inverterMock, Start())
        .WillOnce(Invoke([this]()
            {
                faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);
            }));
    EnableAsync(sm);

    EXPECT_TRUE(std::holds_alternative<state_machine::Fault>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, enableResult);
}

TEST_F(FocStateMachineTorqueAutoTest, an_entry_into_fault_is_announced_with_its_code_once_per_episode)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);
    std::vector<state_machine::FaultCode> announced;
    sm.RegisterFaultEnteredHandler([&announced](state_machine::FaultCode code)
        {
            announced.push_back(code);
        });
    EXPECT_CALL(inverterMock, Start());
    sm.CmdEnable();

    faultNotifierMock.TriggerFault(state_machine::FaultCode::overvoltage);
    faultNotifierMock.TriggerFault(state_machine::FaultCode::overcurrent);

    EXPECT_THAT(announced, ElementsAre(state_machine::FaultCode::overvoltage));
}

TEST_F(FocStateMachineTorqueAutoTest, a_fault_after_a_clear_is_announced_again)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    auto sm = CreateStateMachine();
    std::vector<state_machine::FaultCode> announced;
    sm.RegisterFaultEnteredHandler([&announced](state_machine::FaultCode code)
        {
            announced.push_back(code);
        });

    faultNotifierMock.TriggerFault(state_machine::FaultCode::hardwareFault);
    sm.CmdClearFault();
    faultNotifierMock.TriggerFault(state_machine::FaultCode::stall);

    EXPECT_THAT(announced, ElementsAre(state_machine::FaultCode::hardwareFault, state_machine::FaultCode::stall));
}

TEST_F(FocStateMachineTorqueTest, a_record_with_an_index_angle_boots_to_ready_without_a_rotor_reference)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_FALSE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
}

TEST_F(FocStateMachineTorqueTest, a_record_with_an_index_angle_boots_to_idle_without_an_index_channel)
{
    EXPECT_CALL(indexSearchMock, Available()).WillRepeatedly(Return(false));
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, a_record_without_an_index_angle_boots_to_idle)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachineWithIndex();

    EXPECT_TRUE(std::holds_alternative<state_machine::Idle>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, the_first_enable_searches_the_index_towards_the_stored_angle_and_enables)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();

    ExpectIndexSearch();
    EnableAsync(sm);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_TRUE(sm.HasPendingAsyncWork());
    EXPECT_FALSE(enableResult.has_value());
    EXPECT_EQ(searchConfig.polePairs, 7u);
    EXPECT_FLOAT_EQ(searchConfig.phaseResistance.Value(), 0.5f);
    EXPECT_FLOAT_EQ(searchConfig.busVoltage.Value(), 24.0f);
    ASSERT_TRUE(searchConfig.target.has_value());
    EXPECT_FLOAT_EQ(searchConfig.target->Value(), 2.5f);

    EXPECT_CALL(inverterMock, Start());
    searchDone(foc::Radians{ -1.0f });

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::ok, enableResult);
    EXPECT_FALSE(sm.HasPendingAsyncWork());

    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdDisable());
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);

    EXPECT_CALL(inverterMock, Start());
    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdEnable());
    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, an_index_search_that_finds_nothing_refuses_the_enable_and_the_next_enable_searches_again)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();

    ExpectIndexSearch();
    EnableAsync(sm);
    searchDone(std::nullopt);

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_FALSE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
    EXPECT_EQ(state_machine::CommandResult::calibrationFailed, enableResult);
    EXPECT_FALSE(sm.HasPendingAsyncWork());

    enableResult.reset();
    ExpectIndexSearch();
    EnableAsync(sm);
    EXPECT_CALL(inverterMock, Start());
    searchDone(foc::Radians{ 0.0f });

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::ok, enableResult);
}

TEST_F(FocStateMachineTorqueTest, a_disable_during_the_index_search_abandons_it_and_refuses_the_enable)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();
    ExpectIndexSearch();
    EnableAsync(sm);

    EXPECT_CALL(indexSearchMock, Abort()).Times(AtLeast(1));
    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdDisable());
    searchDone(foc::Radians{ 0.0f });

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::rejected, enableResult);
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, an_emergency_stop_during_the_index_search_aborts_it)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();
    ExpectIndexSearch();
    EnableAsync(sm);

    EXPECT_CALL(indexSearchMock, Abort()).Times(AtLeast(1));
    sm.CmdEmergencyStop();
    searchDone(foc::Radians{ 0.0f });

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_FALSE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
    EXPECT_EQ(state_machine::CommandResult::abortedByFault, enableResult);
    EXPECT_FALSE(sm.HasPendingAsyncWork());
}

TEST_F(FocStateMachineTorqueTest, the_offsets_are_refreshed_before_the_index_is_searched)
{
    GivenFaultNotifierRegistered();
    auto record = RecordWithIndexAngle();
    record.currentOffsetA = 0.010f;
    record.currentOffsetB = -0.020f;
    record.currentOffsetC = 0.030f;
    GivenNvmHolds(record);
    EXPECT_CALL(offsetsMock, Apply(OffsetsNear(0.010f, -0.020f, 0.030f))).Times(AtLeast(1));
    auto sm = CreateStateMachineWithIndexRefreshingOffsets();
    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));

    ExpectRefreshStarted();
    EnableAsync(sm);

    ExpectIndexSearch();
    RefreshMeasures(foc::PhaseCurrents{ foc::Ampere{ 0.010f }, foc::Ampere{ -0.020f }, foc::Ampere{ 0.030f } });
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_FALSE(enableResult.has_value());

    EXPECT_CALL(inverterMock, Start());
    searchDone(foc::Radians{ 0.0f });

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::ok, enableResult);
}

TEST_F(FocStateMachineTorqueTest, a_re_alignment_stores_the_index_angle_the_search_measured)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachineWithIndex();

    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    ExpectIndexSearch();
    sm.CmdReAlign([](state_machine::CommandResult) {});
    EXPECT_FALSE(searchConfig.target.has_value());

    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData& data, infra::Function<void(services::NvmStatus)> onDone)
            {
                ASSERT_TRUE(services::EncoderIndexAngle(data).has_value());
                EXPECT_FLOAT_EQ(*services::EncoderIndexAngle(data), -1.75f);
                onDone(services::NvmStatus::Ok);
            }));
    searchDone(foc::Radians{ -1.75f });

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
}

TEST_F(FocStateMachineTorqueTest, a_drive_is_not_homed_after_boot)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();

    EXPECT_FALSE(sm.IsHomed());
}

TEST_F(FocStateMachineTorqueTest, a_re_alignment_does_not_home_the_drive)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);

    ASSERT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
    EXPECT_FALSE(sm.IsHomed());
    EXPECT_FALSE(std::get<state_machine::Ready>(sm.CurrentState()).homed);
}

TEST_F(FocStateMachineTorqueTest, a_calibration_homes_the_drive)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();

    sm.CmdCalibrate([](state_machine::CommandResult) {});

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_TRUE(sm.IsHomed());
    EXPECT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).homed);
}

TEST_F(FocStateMachineTorqueTest, a_drive_kept_homed_across_a_mode_switch_boots_homed)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    TestedStateMachine sm{ tracer, application::MotorHardware{ inverterMock, encoderMock, vdc }, nvmMock, application::CalibrationServices{ electricalIdentMock, alignmentMock }, faultNotifierMock, nvmActivity, application::RotorReference::keptHomed };

    ASSERT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_TRUE(sm.IsHomed());
    EXPECT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).homed);
}

TEST_F(FocStateMachineTorqueTest, a_drive_kept_aligned_across_a_mode_switch_boots_not_homed)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    TestedStateMachine sm{ tracer, application::MotorHardware{ inverterMock, encoderMock, vdc }, nvmMock, application::CalibrationServices{ electricalIdentMock, alignmentMock }, faultNotifierMock, nvmActivity, application::RotorReference::kept };

    ASSERT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
    EXPECT_FALSE(sm.IsHomed());
}

TEST_F(FocStateMachineTorqueTest, a_restored_index_reference_homes_the_drive_and_a_re_alignment_drops_it)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();
    ASSERT_FALSE(sm.IsHomed());

    ExpectIndexSearch();
    EnableAsync(sm);
    EXPECT_CALL(inverterMock, Start());
    searchDone(foc::Radians{ -1.0f });
    EXPECT_TRUE(sm.IsHomed());

    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdDisable());
    EXPECT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).homed);

    EXPECT_CALL(alignmentMock, ForceAlignment(_, _, _))
        .WillOnce(Invoke([](std::size_t, const auto&, const infra::Function<void(std::optional<foc::Radians>)>& cb)
            {
                cb(foc::Radians{ 0.0f });
            }));
    ExpectIndexSearch();
    sm.CmdReAlign([](state_machine::CommandResult) {});
    EXPECT_CALL(nvmMock, SaveCalibration(_, _))
        .WillOnce(Invoke([](const services::CalibrationData&, infra::Function<void(services::NvmStatus)> onDone)
            {
                onDone(services::NvmStatus::Ok);
            }));
    searchDone(foc::Radians{ -1.75f });

    ASSERT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
    EXPECT_FALSE(sm.IsHomed());
}

TEST_F(FocStateMachineTorqueTest, an_index_search_that_finds_nothing_leaves_the_drive_not_homed)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();

    ExpectIndexSearch();
    EnableAsync(sm);
    searchDone(std::nullopt);

    EXPECT_FALSE(sm.IsHomed());
}

TEST_F(FocStateMachineTorqueTest, required_homing_refuses_an_enable_after_a_plain_alignment)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachine();
    AlignAfterBoot(sm);
    sm.RequireHoming(true);

    EXPECT_EQ(state_machine::CommandResult::rejected, sm.CmdEnable());
    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
}

TEST_F(FocStateMachineTorqueTest, required_homing_lets_an_enable_through_once_the_drive_is_calibrated)
{
    GivenFaultNotifierRegistered();
    GivenNvmInvalid();
    ExpectCalibrationSequence();
    auto sm = CreateStateMachine();
    sm.CmdCalibrate([](state_machine::CommandResult) {});
    sm.RequireHoming(true);

    EXPECT_CALL(inverterMock, Start());
    EXPECT_EQ(state_machine::CommandResult::ok, sm.CmdEnable());
}

TEST_F(FocStateMachineTorqueTest, required_homing_lets_an_enable_through_when_the_index_restore_is_due)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();
    sm.RequireHoming(true);

    ExpectIndexSearch();
    EnableAsync(sm);
    EXPECT_CALL(inverterMock, Start());
    searchDone(foc::Radians{ -1.0f });

    EXPECT_TRUE(std::holds_alternative<state_machine::Enabled>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::ok, enableResult);
}

TEST_F(FocStateMachineTorqueTest, required_homing_keeps_the_refusal_of_a_failed_index_search)
{
    GivenFaultNotifierRegistered();
    GivenNvmHolds(RecordWithIndexAngle());
    auto sm = CreateStateMachineWithIndex();
    sm.RequireHoming(true);

    ExpectIndexSearch();
    EnableAsync(sm);
    searchDone(std::nullopt);

    EXPECT_TRUE(std::holds_alternative<state_machine::Ready>(sm.CurrentState()));
    EXPECT_EQ(state_machine::CommandResult::calibrationFailed, enableResult);
}

TEST_F(FocStateMachineTorqueTest, required_homing_refuses_an_enable_after_an_alignment_even_with_an_index_to_restore)
{
    GivenFaultNotifierRegistered();
    GivenNvmValid();
    auto sm = CreateStateMachineWithIndex();
    ExpectIndexSearch();
    AlignAfterBoot(sm);
    searchDone(foc::Radians{ -1.75f });
    sm.RequireHoming(true);

    ASSERT_TRUE(std::get<state_machine::Ready>(sm.CurrentState()).rotorReferenceValid);
    EXPECT_EQ(state_machine::CommandResult::rejected, sm.CmdEnable());
}
