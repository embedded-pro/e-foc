#include "tools/virtual_motor/app/logic/CaptureCsv.hpp"
#include "gtest/gtest.h"
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::vector<std::string> Lines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::istringstream stream{ text };
        for (std::string line; std::getline(stream, line);)
            lines.push_back(line);
        return lines;
    }

    std::vector<std::string> Fields(const std::string& line)
    {
        std::vector<std::string> fields;
        std::istringstream stream{ line };
        for (std::string field; std::getline(stream, field, ',');)
            fields.push_back(field);
        return fields;
    }
}

TEST(TestCaptureCsv, heads_every_column_with_its_name_and_unit)
{
    virtual_motor::app::Capture capture{ 1, 0, 0, { virtual_motor::port::Sample{} } };
    const auto header = Fields(Lines(virtual_motor::app::CaptureCsv(capture, 20000)).front());

    ASSERT_EQ(header.size(), 2 + virtual_motor::port::signalCount);
    EXPECT_EQ(header[0], "time [s]");
    EXPECT_EQ(header[1], "control period");
    EXPECT_EQ(header[2], "Ia [A]");
    EXPECT_EQ(header[20], "ω [rad/s]");
    EXPECT_EQ(header[26], "T winding [°C]");
}

TEST(TestCaptureCsv, writes_one_row_per_period_timed_from_the_trigger)
{
    virtual_motor::app::Capture capture{ 1, 2, 1000, std::vector<virtual_motor::port::Sample>(4) };
    capture.periods[3][0] = 1.5f;
    capture.periods[3][24] = 40.25f;

    const auto lines = Lines(virtual_motor::app::CaptureCsv(capture, 20000));
    ASSERT_EQ(lines.size(), 5u);

    const auto first = Fields(lines[1]);
    EXPECT_NEAR(std::stod(first[0]), -100e-6, 1e-12);
    EXPECT_EQ(first[1], "1000");

    const auto last = Fields(lines[4]);
    EXPECT_NEAR(std::stod(last[0]), 50e-6, 1e-12);
    EXPECT_EQ(last[1], "1003");
    EXPECT_NEAR(std::stod(last[2]), 1.5, 1e-6);
    EXPECT_NEAR(std::stod(last[26]), 40.25, 1e-6);
}
