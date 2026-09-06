#include "core/platform_abstraction/test_doubles/PlatformFactoryMock.hpp"
#include "core/services/cli/TerminalDiagnostics.hpp"
#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "infra/util/ByteRange.hpp"
#include "services/util/Terminal.hpp"
#include "gmock/gmock.h"
#include <string>

namespace
{
    using CanError = application::CanBusAdapter::CanError;

    class TerminalDiagnosticsTest
        : public ::testing::Test
        , public infra::EventDispatcherWithWeakPtrFixture
    {
    public:
        infra::StringOutputStream::WithStorage<4096> stream{ infra::noFail };
        services::TracerToStream tracer{ stream };
        ::testing::StrictMock<hal::SerialCommunicationMock> communication;
        services::TerminalWithCommandsImpl::WithMaxQueueAndMaxHistory<128, 5> terminalWithCommands{ communication, tracer };
        services::TerminalWithStorage::WithMaxSize<10> terminal{ terminalWithCommands, tracer };
        ::testing::StrictMock<application::PlatformFactoryMock> platform;
        application::CanBusAdapter::ErrorCounters counters;
        services::TerminalDiagnostics diagnostics{ terminal, platform };

        void InvokeCommand(const std::string& command)
        {
            stream.Storage().clear();
            communication.dataReceived(infra::MakeStringByteRange(command + "\r"));
            ExecuteAllActions();
        }

        std::string Output() const
        {
            return { stream.Storage().begin(), stream.Storage().end() };
        }

        void RecordError(CanError error)
        {
            counters.Record(error);
        }
    };
}

TEST_F(TerminalDiagnosticsTest, loop_stats_reports_the_execution_statistics)
{
    application::ControlLoopMetrics::Snapshot snapshot{ 1234u, 2100u, 1800u, 4700u, 2050u, 4500u, 7u, 3u, 1u };

    EXPECT_CALL(platform, ControlLoopStatistics()).WillOnce(::testing::Return(snapshot));
    EXPECT_CALL(platform, Tracer()).WillOnce(::testing::ReturnRef(tracer));

    InvokeCommand("loop_stats");

    const auto output = Output();
    EXPECT_THAT(output, ::testing::HasSubstr("samples=1234"));
    EXPECT_THAT(output, ::testing::HasSubstr("budget=4500"));
    EXPECT_THAT(output, ::testing::HasSubstr("last=2100"));
    EXPECT_THAT(output, ::testing::HasSubstr("min=1800"));
    EXPECT_THAT(output, ::testing::HasSubstr("avg=2050"));
    EXPECT_THAT(output, ::testing::HasSubstr("max=4700"));
    EXPECT_THAT(output, ::testing::HasSubstr("overruns=7"));
    EXPECT_THAT(output, ::testing::HasSubstr("deadlineMisses=3"));
    EXPECT_THAT(output, ::testing::HasSubstr("reentries=1"));
}

TEST_F(TerminalDiagnosticsTest, loop_stats_reports_zeroes_on_a_platform_that_does_not_measure)
{
    EXPECT_CALL(platform, ControlLoopStatistics()).WillOnce(::testing::Return(application::ControlLoopMetrics::Snapshot{}));
    EXPECT_CALL(platform, Tracer()).WillOnce(::testing::ReturnRef(tracer));

    InvokeCommand("ls");

    EXPECT_THAT(Output(), ::testing::HasSubstr("samples=0"));
}

TEST_F(TerminalDiagnosticsTest, can_stats_prints_only_the_classes_that_have_occurred)
{
    RecordError(CanError::busOff);
    RecordError(CanError::crcError);
    RecordError(CanError::crcError);

    EXPECT_CALL(platform, CanStatistics()).WillOnce(::testing::ReturnRef(counters));
    EXPECT_CALL(platform, Tracer()).WillOnce(::testing::ReturnRef(tracer));

    InvokeCommand("can_stats");

    const auto output = Output();
    EXPECT_THAT(output, ::testing::HasSubstr("total errors=3"));
    EXPECT_THAT(output, ::testing::HasSubstr("bus off=1"));
    EXPECT_THAT(output, ::testing::HasSubstr("crc error=2"));
    EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("stuff error")));
    EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("bit1 error")));
}

TEST_F(TerminalDiagnosticsTest, can_stats_on_a_quiet_bus_prints_the_total_and_nothing_else)
{
    EXPECT_CALL(platform, CanStatistics()).WillOnce(::testing::ReturnRef(counters));
    EXPECT_CALL(platform, Tracer()).WillOnce(::testing::ReturnRef(tracer));

    InvokeCommand("cs");

    const auto output = Output();
    EXPECT_THAT(output, ::testing::HasSubstr("total errors=0"));
    EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("bus off")));
}

TEST_F(TerminalDiagnosticsTest, clear_stats_resets_both_sets_of_counters)
{
    EXPECT_CALL(platform, ResetStatistics());

    InvokeCommand("clear_stats");
}

TEST_F(TerminalDiagnosticsTest, an_unknown_class_is_reported_as_other_rather_than_indexing_past_the_end)
{
    RecordError(static_cast<CanError>(99));

    EXPECT_CALL(platform, CanStatistics()).WillOnce(::testing::ReturnRef(counters));
    EXPECT_CALL(platform, Tracer()).WillOnce(::testing::ReturnRef(tracer));

    InvokeCommand("can_stats");

    EXPECT_THAT(Output(), ::testing::HasSubstr("other=1"));
}
