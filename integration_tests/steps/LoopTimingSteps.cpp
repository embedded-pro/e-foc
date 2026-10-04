#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>
#include <optional>
#include <string>

using namespace integration;

namespace
{
    constexpr auto kTraceTimeout = std::chrono::seconds{ 15 };

    struct LoopStatistics
    {
        unsigned long samples{ 0 };
        unsigned long budget{ 0 };
        unsigned long minimum{ 0 };
        unsigned long average{ 0 };
        unsigned long maximum{ 0 };
        unsigned long overruns{ 0 };
        unsigned long deadlineMisses{ 0 };
        unsigned long reentries{ 0 };
    };

    std::optional<unsigned long> ValueAfter(const std::string& line, const std::string& key)
    {
        const auto at = line.find(key);
        if (at == std::string::npos)
            return std::nullopt;
        return std::strtoul(line.c_str() + at + key.size(), nullptr, 10);
    }

    std::optional<std::string> LastLineWith(std::size_t fromLine, const std::string& needle)
    {
        const auto& lines = TargetInteractor::Instance().SerialLines();
        std::optional<std::string> found;
        for (std::size_t i = fromLine; i < lines.size(); ++i)
            if (lines[i].find(needle) != std::string::npos)
                found = lines[i];
        return found;
    }

    template<typename Predicate>
    bool DrainUntil(Fixture& fixture, Predicate done, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (done())
                return true;
            fixture.DrainLines(std::chrono::milliseconds{ 50 });
        }
        return done();
    }

    std::optional<LoopStatistics> ReadLoopStatistics(Fixture& fixture)
    {
        const auto mark = fixture.CapturedLineCount();
        if (!fixture.SendCommand("loop_stats"))
            return std::nullopt;

        std::optional<std::string> totals;
        std::optional<std::string> durations;
        std::optional<std::string> counters;
        const bool printed = DrainUntil(fixture, [&]
            {
                totals = LastLineWith(mark, "[LOOP] samples=");
                durations = LastLineWith(mark, "[LOOP] cycles last=");
                counters = LastLineWith(mark, "[LOOP] overruns=");
                return totals.has_value() && durations.has_value() && counters.has_value();
            },
            kTraceTimeout);

        if (!printed)
            return std::nullopt;

        LoopStatistics statistics;
        statistics.samples = ValueAfter(*totals, "samples=").value_or(0);
        statistics.budget = ValueAfter(*totals, "budget=").value_or(0);
        statistics.minimum = ValueAfter(*durations, "min=").value_or(0);
        statistics.average = ValueAfter(*durations, "avg=").value_or(0);
        statistics.maximum = ValueAfter(*durations, "max=").value_or(0);
        statistics.overruns = ValueAfter(*counters, "overruns=").value_or(0);
        statistics.deadlineMisses = ValueAfter(*counters, "deadlineMisses=").value_or(0);
        statistics.reentries = ValueAfter(*counters, "reentries=").value_or(0);
        return statistics;
    }
}

WHEN(R"(the control loop statistics are cleared)")
{
    ASSERT_TRUE(context.Get<Fixture>().SendCommand("clear_stats")) << "The clear_stats command was not acknowledged";
}

THEN(R"(the control loop shall have stayed within its budget)")
{
    const auto statistics = ReadLoopStatistics(context.Get<Fixture>());
    ASSERT_TRUE(statistics.has_value()) << "The target did not print its control loop statistics";

    std::fprintf(stderr, "[METRIC] loop samples=%lu budget=%lu min=%lu avg=%lu max=%lu overruns=%lu deadlineMisses=%lu reentries=%lu\n",
        statistics->samples, statistics->budget, statistics->minimum, statistics->average, statistics->maximum,
        statistics->overruns, statistics->deadlineMisses, statistics->reentries);

    ASSERT_GT(statistics->samples, 0u) << "The control interrupt never ran";
    EXPECT_GT(statistics->minimum, 0u) << "Every execution measured zero, so the cycle counter is not running";
    EXPECT_LE(statistics->maximum, statistics->budget) << "The slowest execution exceeded the budget";
    EXPECT_EQ(statistics->overruns, 0u) << "Executions exceeded the budget";
    EXPECT_EQ(statistics->deadlineMisses, 0u) << "Executions exceeded the control period";
    EXPECT_EQ(statistics->reentries, 0u) << "The control interrupt re-entered";
}

THEN(R"(no control loop execution shall have taken more than {int} cycles)", (int cycles))
{
    const auto statistics = ReadLoopStatistics(context.Get<Fixture>());
    ASSERT_TRUE(statistics.has_value()) << "The target did not print its control loop statistics";

    EXPECT_LE(statistics->maximum, static_cast<unsigned long>(cycles))
        << "The slowest execution took " << statistics->maximum << " cycles";
}
