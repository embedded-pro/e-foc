#include "tools/virtual_motor/engine/Supervisor.hpp"
#include "gtest/gtest.h"
#include <string>
#include <vector>

namespace
{
    using Arguments = std::vector<std::string>;
}

TEST(TestSupervisor, restart_marks_the_motor_restarted_and_keeps_its_memory)
{
    EXPECT_EQ(virtual_motor::RestartArguments({ "e-foc-virtual-motor", "--bus", "a", "--factory-fresh" }),
        (Arguments{ "e-foc-virtual-motor", "--bus", "a", "--restarted" }));
}

TEST(TestSupervisor, restart_marks_a_restarted_motor_once)
{
    EXPECT_EQ(virtual_motor::RestartArguments({ "e-foc-virtual-motor", "--restarted" }),
        (Arguments{ "e-foc-virtual-motor", "--restarted" }));
}

TEST(TestSupervisor, quotes_only_arguments_that_need_it)
{
    EXPECT_EQ(virtual_motor::WindowsQuoted("--bus"), "--bus");
    EXPECT_EQ(virtual_motor::WindowsQuoted(R"(C:\motors\a.nvm)"), R"(C:\motors\a.nvm)");
    EXPECT_EQ(virtual_motor::WindowsQuoted(""), R"("")");
    EXPECT_EQ(virtual_motor::WindowsQuoted(R"(C:\Program Files\e-foc\plant.txt)"), R"("C:\Program Files\e-foc\plant.txt")");
}

TEST(TestSupervisor, escapes_quotes_and_the_backslashes_before_them)
{
    EXPECT_EQ(virtual_motor::WindowsQuoted(R"(say "hi")"), R"("say \"hi\"")");
    EXPECT_EQ(virtual_motor::WindowsQuoted(R"(a\"b c)"), R"("a\\\"b c")");
    EXPECT_EQ(virtual_motor::WindowsQuoted(R"(C:\my dir\)"), R"("C:\my dir\\")");
}

TEST(TestSupervisor, runs_the_motor_supervised_and_returns_how_it_ended)
{
    std::vector<Arguments> runs;
    const auto code = virtual_motor::Supervise({ "e-foc-virtual-motor", "--bus", "a" }, [&runs](const Arguments& arguments)
        {
            runs.push_back(arguments);
            return 3;
        });

    EXPECT_EQ(code, 3);
    EXPECT_EQ(runs, (std::vector<Arguments>{ { "e-foc-virtual-motor", "--bus", "a", "--supervised" } }));
}

TEST(TestSupervisor, starts_a_motor_that_asks_to_restart_again_as_restarted)
{
    std::vector<Arguments> runs;
    const auto code = virtual_motor::Supervise({ "e-foc-virtual-motor", "--factory-fresh" }, [&runs](const Arguments& arguments)
        {
            runs.push_back(arguments);
            return runs.size() < 3 ? virtual_motor::restartExitCode : 0;
        });

    EXPECT_EQ(code, 0);
    EXPECT_EQ(runs, (std::vector<Arguments>{
                        { "e-foc-virtual-motor", "--factory-fresh", "--supervised" },
                        { "e-foc-virtual-motor", "--supervised", "--restarted" },
                        { "e-foc-virtual-motor", "--supervised", "--restarted" },
                    }));
}

TEST(TestSupervisor, restart_keeps_the_memory_a_calibrated_start_wrote)
{
    EXPECT_EQ(virtual_motor::RestartArguments({ "e-foc-virtual-motor", "--calibrated" }),
        (Arguments{ "e-foc-virtual-motor", "--restarted" }));
}
