#include "TestFocStateMachineHelper.hpp"
#include "core/state_machine/CalibrationOrchestrator.hpp"

namespace
{
    using namespace testing;

    class CalibrationOrchestratorTest
        : public ::testing::Test
    {
    public:
        StrictMock<services::ElectricalParametersIdentificationMock> electricalIdent;
        StrictMock<services::MotorAlignmentMock> motorAlignment;
        StrictMock<infra::StreamWriterMock> streamWriter;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriter };
        services::TracerToStream tracer{ stream };
        application::CalibrationOrchestrator orchestrator{ electricalIdent, motorAlignment, tracer };
        services::CalibrationData pendingData{};

        infra::Function<void(std::optional<std::size_t>)> storedPolePairsCb;
        infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)> storedRlCb;
        infra::Function<void(std::optional<foc::Radians>)> storedAlignCb;

        bool alignedCalled{ false };
        bool failedCalled{ false };
        foc::Radians receivedAngle{ 0.0f };

        infra::Execute suppressTracerExpectations{ [this]()
            {
                EXPECT_CALL(streamWriter, Insert(_, _)).Times(AnyNumber());
                EXPECT_CALL(streamWriter, Available()).Times(AnyNumber()).WillRepeatedly(Return(1000));
                EXPECT_CALL(streamWriter, ConstructSaveMarker()).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriter, GetProcessedBytesSince(_)).Times(AnyNumber()).WillRepeatedly(Return(0));
                EXPECT_CALL(streamWriter, SaveState(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
                EXPECT_CALL(streamWriter, RestoreState(_)).Times(AnyNumber());
                EXPECT_CALL(streamWriter, Overwrite(_)).Times(AnyNumber()).WillRepeatedly(Return(infra::ByteRange{}));
            } };

        void StartOrchestrator()
        {
            orchestrator.Start(
                pendingData,
                [](state_machine::CalibrationStep) {},
                [this](foc::Radians angle)
                {
                    alignedCalled = true;
                    receivedAngle = angle;
                },
                [this]
                {
                    failedCalled = true;
                });
        }

        void StartAlignmentOnly()
        {
            pendingData.polePairs = 4;
            orchestrator.StartAlignmentOnly(
                pendingData,
                [](state_machine::CalibrationStep) {},
                [this](foc::Radians angle)
                {
                    alignedCalled = true;
                    receivedAngle = angle;
                },
                [this]
                {
                    failedCalled = true;
                });
        }

        void ExpectPolePairCall()
        {
            EXPECT_CALL(electricalIdent, EstimateNumberOfPolePairs(_, _))
                .WillOnce(Invoke([this](auto, const infra::Function<void(std::optional<std::size_t>)>& cb)
                    {
                        storedPolePairsCb = cb;
                    }));
        }

        void ExpectRlCall()
        {
            EXPECT_CALL(electricalIdent, EstimateResistanceAndInductance(_, _))
                .WillOnce(Invoke([this](auto, const infra::Function<void(services::ElectricalParametersIdentification::ResistanceInductanceResult)>& cb)
                    {
                        storedRlCb = cb;
                    }));
        }

        void ExpectAlignCall()
        {
            EXPECT_CALL(motorAlignment, ForceAlignment(_, _, _))
                .WillOnce(Invoke([this](auto, auto, const infra::Function<void(std::optional<foc::Radians>)>& cb)
                    {
                        storedAlignCb = cb;
                    }));
        }

        services::ElectricalParametersIdentification::ResistanceInductanceResult ValidRlResult()
        {
            services::ElectricalParametersIdentification::ResistanceInductanceResult result{};
            result.resistance = foc::Ohm{ 0.5f };
            result.inductance = foc::MilliHenry{ 1.0f };
            result.fitQuality = 0.9f;
            return result;
        }
    };
}

TEST_F(CalibrationOrchestratorTest, full_sequence_success)
{
    ExpectPolePairCall();
    StartOrchestrator();

    ExpectRlCall();
    storedPolePairsCb(std::size_t{ 4 });

    ExpectAlignCall();
    storedRlCb(ValidRlResult());

    storedAlignCb(foc::Radians{ 1.5f });

    EXPECT_TRUE(alignedCalled);
    EXPECT_FALSE(failedCalled);
    EXPECT_NEAR(receivedAngle.Value(), 1.5f, 1e-6f);
    EXPECT_EQ(pendingData.polePairs, 4);
    EXPECT_NEAR(pendingData.rPhase, 0.5f, 1e-6f);
}

TEST_F(CalibrationOrchestratorTest, pole_pairs_failure_triggers_onFailed)
{
    ExpectPolePairCall();
    StartOrchestrator();

    storedPolePairsCb(std::nullopt);

    EXPECT_FALSE(alignedCalled);
    EXPECT_TRUE(failedCalled);
}

TEST_F(CalibrationOrchestratorTest, rl_failure_triggers_onFailed)
{
    ExpectPolePairCall();
    StartOrchestrator();
    ExpectRlCall();
    storedPolePairsCb(std::size_t{ 4 });

    services::ElectricalParametersIdentification::ResistanceInductanceResult badResult{};
    storedRlCb(badResult);

    EXPECT_FALSE(alignedCalled);
    EXPECT_TRUE(failedCalled);
}

TEST_F(CalibrationOrchestratorTest, alignment_failure_triggers_onFailed)
{
    ExpectPolePairCall();
    StartOrchestrator();
    ExpectRlCall();
    storedPolePairsCb(std::size_t{ 4 });
    ExpectAlignCall();
    storedRlCb(ValidRlResult());

    storedAlignCb(std::nullopt);

    EXPECT_FALSE(alignedCalled);
    EXPECT_TRUE(failedCalled);
}

TEST_F(CalibrationOrchestratorTest, abort_before_pole_pairs_callback_suppresses_result)
{
    EXPECT_CALL(electricalIdent, Abort());
    EXPECT_CALL(motorAlignment, Abort());
    EXPECT_CALL(electricalIdent, EstimateNumberOfPolePairs(_, _))
        .WillOnce(Invoke([this](auto, const infra::Function<void(std::optional<std::size_t>)>& cb)
            {
                storedPolePairsCb = cb;
            }));
    StartOrchestrator();

    orchestrator.Abort();
    storedPolePairsCb(std::size_t{ 4 });

    EXPECT_FALSE(alignedCalled);
    EXPECT_FALSE(failedCalled);
}

TEST_F(CalibrationOrchestratorTest, abort_before_rl_callback_suppresses_result)
{
    ExpectPolePairCall();
    StartOrchestrator();
    ExpectRlCall();
    storedPolePairsCb(std::size_t{ 4 });

    EXPECT_CALL(electricalIdent, Abort());
    EXPECT_CALL(motorAlignment, Abort());
    orchestrator.Abort();
    storedRlCb(ValidRlResult());

    EXPECT_FALSE(alignedCalled);
    EXPECT_FALSE(failedCalled);
}

TEST_F(CalibrationOrchestratorTest, abort_before_alignment_callback_suppresses_result)
{
    ExpectPolePairCall();
    StartOrchestrator();
    ExpectRlCall();
    storedPolePairsCb(std::size_t{ 4 });
    ExpectAlignCall();
    storedRlCb(ValidRlResult());

    EXPECT_CALL(electricalIdent, Abort());
    EXPECT_CALL(motorAlignment, Abort());
    orchestrator.Abort();
    storedAlignCb(foc::Radians{ 1.5f });

    EXPECT_FALSE(alignedCalled);
    EXPECT_FALSE(failedCalled);
}

TEST_F(CalibrationOrchestratorTest, abort_calls_both_service_aborts)
{
    EXPECT_CALL(electricalIdent, Abort());
    EXPECT_CALL(motorAlignment, Abort());
    orchestrator.Abort();
}

TEST_F(CalibrationOrchestratorTest, start_alignment_only_skips_electrical_ident)
{
    ExpectAlignCall();
    StartAlignmentOnly();

    storedAlignCb(foc::Radians{ 0.5f });

    EXPECT_TRUE(alignedCalled);
    EXPECT_FALSE(failedCalled);
    EXPECT_NEAR(receivedAngle.Value(), 0.5f, 1e-6f);
}

TEST_F(CalibrationOrchestratorTest, is_running_delegates_to_electrical_ident)
{
    EXPECT_CALL(electricalIdent, IsRunning()).WillOnce(Return(true));
    EXPECT_TRUE(orchestrator.IsRunning());
}

TEST_F(CalibrationOrchestratorTest, stale_alignment_callback_after_new_run_is_suppressed)
{
    ExpectPolePairCall();
    StartOrchestrator();
    ExpectRlCall();
    storedPolePairsCb(std::size_t{ 4 });
    ExpectAlignCall();
    storedRlCb(ValidRlResult());

    const auto staleAlignCb = storedAlignCb;

    EXPECT_CALL(electricalIdent, Abort());
    EXPECT_CALL(motorAlignment, Abort());
    orchestrator.Abort();

    ExpectPolePairCall();
    StartOrchestrator();

    staleAlignCb(foc::Radians{ 1.5f });

    EXPECT_FALSE(alignedCalled);
    EXPECT_FALSE(failedCalled);
}
