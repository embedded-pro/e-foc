#include "can-lite/core/test/CanMock.hpp"
#include "core/platform_abstraction/PlatformDiagnostics.hpp"
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

    constexpr uint32_t budget = 4500;
    constexpr uint32_t period = 6000;

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

        application::ControlLoopMetrics metrics;
        application::CanBusAdapterImpl<::testing::StrictMock<hal::CanMock>> canBus;
        application::PlatformDiagnostics diagnostics{ metrics };
        services::TerminalDiagnostics terminalDiagnostics{ terminal, diagnostics, tracer };

        TerminalDiagnosticsTest()
        {
            metrics.Configure(budget, period);
        }

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
    };
}

TEST_F(TerminalDiagnosticsTest, loop_stats_reports_the_execution_statistics)
{
    metrics.Record(2100);
    metrics.Record(4700);
    metrics.Record(6100);

    InvokeCommand("loop_stats");

    const auto output = Output();
    EXPECT_THAT(output, ::testing::HasSubstr("samples=3"));
    EXPECT_THAT(output, ::testing::HasSubstr("budget=4500"));
    EXPECT_THAT(output, ::testing::HasSubstr("last=6100"));
    EXPECT_THAT(output, ::testing::HasSubstr("min=2100"));
    EXPECT_THAT(output, ::testing::HasSubstr("max=6100"));
    EXPECT_THAT(output, ::testing::HasSubstr("overruns=2"));
    EXPECT_THAT(output, ::testing::HasSubstr("deadlineMisses=1"));
    EXPECT_THAT(output, ::testing::HasSubstr("reentries=0"));
}

TEST_F(TerminalDiagnosticsTest, loop_stats_before_any_sample_reports_zeroes)
{
    InvokeCommand("ls");

    const auto output = Output();
    EXPECT_THAT(output, ::testing::HasSubstr("samples=0"));
    EXPECT_THAT(output, ::testing::HasSubstr("min=0"));
    EXPECT_THAT(output, ::testing::HasSubstr("max=0"));
}

TEST_F(TerminalDiagnosticsTest, can_stats_prints_only_the_classes_that_have_occurred)
{
    diagnostics.AttachCanBus(canBus);
    canBus.InvokeErrorHandler(CanError::busOff);
    canBus.InvokeErrorHandler(CanError::crcError);
    canBus.InvokeErrorHandler(CanError::crcError);

    InvokeCommand("can_stats");

    const auto output = Output();
    EXPECT_THAT(output, ::testing::HasSubstr("total errors=3"));
    EXPECT_THAT(output, ::testing::HasSubstr("bus off=1"));
    EXPECT_THAT(output, ::testing::HasSubstr("crc error=2"));
    EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("stuff error")));
    EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("bit1 error")));
}

TEST_F(TerminalDiagnosticsTest, can_stats_before_a_bus_is_configured_reports_a_quiet_bus)
{
    InvokeCommand("cs");

    const auto output = Output();
    EXPECT_THAT(output, ::testing::HasSubstr("total errors=0"));
    EXPECT_THAT(output, ::testing::Not(::testing::HasSubstr("bus off")));
}

TEST_F(TerminalDiagnosticsTest, clear_stats_resets_both_sets_of_counters)
{
    diagnostics.AttachCanBus(canBus);
    metrics.Record(2100);
    canBus.InvokeErrorHandler(CanError::busOff);

    InvokeCommand("clear_stats");

    EXPECT_EQ(0u, diagnostics.ControlLoopStatistics().samples);
    EXPECT_EQ(0u, diagnostics.CanStatistics().Total());
}

TEST_F(TerminalDiagnosticsTest, clear_stats_without_a_bus_still_clears_the_loop_counters)
{
    metrics.Record(2100);

    InvokeCommand("clear_stats");

    EXPECT_EQ(0u, diagnostics.ControlLoopStatistics().samples);
}

TEST_F(TerminalDiagnosticsTest, an_unknown_class_is_reported_as_other_rather_than_indexing_past_the_end)
{
    diagnostics.AttachCanBus(canBus);
    canBus.InvokeErrorHandler(static_cast<CanError>(99));

    InvokeCommand("can_stats");

    EXPECT_THAT(Output(), ::testing::HasSubstr("other=1"));
}
