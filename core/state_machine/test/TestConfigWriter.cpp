#include "TestFocStateMachineHelper.hpp"
#include "core/state_machine/ConfigWriter.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include <optional>

namespace
{
    using namespace testing;

    class ConfigWriterTest
        : public ::testing::Test
        , public infra::ClockFixture
    {
    public:
        StrictMock<services::NonVolatileMemoryMock> nvm;
        StrictMock<infra::StreamWriterMock> streamWriter;
        infra::TextOutputStream::WithErrorPolicy stream{ streamWriter };
        services::TracerToStream tracer{ stream };
        services::ConfigData config{};
        application::NvmActivity nvmActivity;
        state_machine::ConfigWriter writer{ nvm, config, tracer, nvmActivity };

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

        infra::Function<void(services::NvmStatus)> pendingWrite;
        std::optional<services::ConfigData> written;

        void ExpectWriteHeld()
        {
            EXPECT_CALL(nvm, SaveConfig(_, _))
                .WillOnce([this](const services::ConfigData& data, infra::Function<void(services::NvmStatus)> onDone)
                    {
                        written = data;
                        pendingWrite = onDone;
                    });
        }
    };
}

TEST_F(ConfigWriterTest, a_write_reports_its_outcome_once_it_lands)
{
    ExpectWriteHeld();
    std::optional<services::NvmStatus> outcome;
    writer.Persist([&outcome](services::NvmStatus status)
        {
            outcome = status;
        });

    EXPECT_TRUE(writer.HasPendingPersist());
    EXPECT_TRUE(nvmActivity.InFlight());
    EXPECT_FALSE(outcome.has_value());

    pendingWrite(services::NvmStatus::Ok);

    EXPECT_EQ(services::NvmStatus::Ok, outcome);
    EXPECT_FALSE(writer.HasPendingPersist());
    EXPECT_FALSE(nvmActivity.InFlight());
}

TEST_F(ConfigWriterTest, a_busy_write_is_retried_before_its_outcome_is_reported)
{
    ExpectWriteHeld();
    std::optional<services::NvmStatus> outcome;
    writer.Persist([&outcome](services::NvmStatus status)
        {
            outcome = status;
        });

    pendingWrite(services::NvmStatus::Busy);
    EXPECT_FALSE(outcome.has_value());
    EXPECT_TRUE(nvmActivity.InFlight());

    ExpectWriteHeld();
    ForwardTime(std::chrono::milliseconds{ 10 });
    pendingWrite(services::NvmStatus::Ok);

    EXPECT_EQ(services::NvmStatus::Ok, outcome);
    EXPECT_FALSE(nvmActivity.InFlight());
}

TEST_F(ConfigWriterTest, a_change_made_during_a_write_is_written_after_it)
{
    ExpectWriteHeld();
    std::optional<services::NvmStatus> first;
    writer.Persist([&first](services::NvmStatus status)
        {
            first = status;
        });

    config.encoderResolution = 8192;
    std::optional<services::NvmStatus> second;
    writer.Persist([&second](services::NvmStatus status)
        {
            second = status;
        });

    EXPECT_EQ(4000u, written->encoderResolution);

    auto firstWrite = pendingWrite;
    ExpectWriteHeld();
    firstWrite(services::NvmStatus::Ok);

    EXPECT_EQ(services::NvmStatus::Ok, first);
    EXPECT_FALSE(second.has_value());
    EXPECT_EQ(8192u, written->encoderResolution);
    EXPECT_TRUE(nvmActivity.InFlight());

    pendingWrite(services::NvmStatus::Ok);

    EXPECT_EQ(services::NvmStatus::Ok, second);
    EXPECT_FALSE(writer.HasPendingPersist());
    EXPECT_FALSE(nvmActivity.InFlight());
}

TEST_F(ConfigWriterTest, a_third_request_waiting_for_an_answer_is_told_busy)
{
    ExpectWriteHeld();
    writer.Persist();
    writer.Persist([](services::NvmStatus) {});

    std::optional<services::NvmStatus> third;
    writer.Persist([&third](services::NvmStatus status)
        {
            third = status;
        });

    EXPECT_EQ(services::NvmStatus::Busy, third);

    auto firstWrite = pendingWrite;
    ExpectWriteHeld();
    firstWrite(services::NvmStatus::Ok);
    pendingWrite(services::NvmStatus::Ok);
    EXPECT_FALSE(nvmActivity.InFlight());
}

TEST_F(ConfigWriterTest, a_failed_write_is_reported_and_releases_the_memory)
{
    ExpectWriteHeld();
    std::optional<services::NvmStatus> outcome;
    writer.Persist([&outcome](services::NvmStatus status)
        {
            outcome = status;
        });

    pendingWrite(services::NvmStatus::WriteFailed);

    EXPECT_EQ(services::NvmStatus::WriteFailed, outcome);
    EXPECT_FALSE(nvmActivity.InFlight());
}
