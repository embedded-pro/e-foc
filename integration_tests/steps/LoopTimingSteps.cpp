#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/CanParameters.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <optional>

using namespace integration;

namespace
{
    constexpr auto kClearTimeout = std::chrono::seconds{ 5 };

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

    std::optional<LoopStatistics> ReadLoopStatistics(Fixture& fixture)
    {
        LoopStatistics statistics;
        const std::array<std::pair<can::FocParameter, unsigned long*>, 8> fields{ {
            { can::FocParameter::loopSamples, &statistics.samples },
            { can::FocParameter::loopBudgetCycles, &statistics.budget },
            { can::FocParameter::loopMinimumCycles, &statistics.minimum },
            { can::FocParameter::loopAverageCycles, &statistics.average },
            { can::FocParameter::loopMaximumCycles, &statistics.maximum },
            { can::FocParameter::loopOverruns, &statistics.overruns },
            { can::FocParameter::loopDeadlineMisses, &statistics.deadlineMisses },
            { can::FocParameter::loopReentries, &statistics.reentries },
        } };

        for (const auto& [parameter, field] : fields)
        {
            const auto value = ReadParameterValue(fixture, parameter);
            if (!value.has_value())
                return std::nullopt;
            *field = static_cast<unsigned long>(std::lround(*value));
        }

        return statistics;
    }
}

WHEN(R"(the control loop statistics are cleared)")
{
    ASSERT_TRUE(context.Get<Fixture>().SendCanCommand(can::focMotorCategoryId, can::focClearDiagnosticsId, {}, kClearTimeout))
        << "The target did not acknowledge ClearDiagnostics";
}

THEN(R"(the control loop shall have stayed within its budget)")
{
    const auto statistics = ReadLoopStatistics(context.Get<Fixture>());
    ASSERT_TRUE(statistics.has_value()) << "The target did not report its control loop statistics over CAN";

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
    ASSERT_TRUE(statistics.has_value()) << "The target did not report its control loop statistics over CAN";

    EXPECT_LE(statistics->maximum, static_cast<unsigned long>(cycles))
        << "The slowest execution took " << statistics->maximum << " cycles";
}
