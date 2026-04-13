#include "core/platform_abstraction/ResetCause.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include <gtest/gtest.h>

namespace
{
    class TestResetCauseStream : public ::testing::Test
    {
    protected:
        infra::BoundedString::WithStorage<64> output;

        std::string StreamCause(application::ResetCause cause)
        {
            output.clear();
            infra::StringOutputStream stream(output, infra::noFail);
            stream << cause;
            return std::string(output.begin(), output.end());
        }
    };
}

TEST_F(TestResetCauseStream, stream_outputs_human_readable_name_for_power_up)
{
    EXPECT_EQ(StreamCause(application::ResetCause::powerUp), "PowerUp");
}

TEST_F(TestResetCauseStream, stream_outputs_human_readable_name_for_software)
{
    EXPECT_EQ(StreamCause(application::ResetCause::software), "Software");
}

TEST_F(TestResetCauseStream, stream_outputs_human_readable_name_for_watchdog)
{
    EXPECT_EQ(StreamCause(application::ResetCause::watchdog), "Watchdog");
}

TEST_F(TestResetCauseStream, stream_outputs_human_readable_name_for_brown_out)
{
    EXPECT_EQ(StreamCause(application::ResetCause::brownOut), "BrownOut");
}

TEST_F(TestResetCauseStream, stream_outputs_human_readable_name_for_hardware)
{
    EXPECT_EQ(StreamCause(application::ResetCause::hardware), "Hardware");
}
