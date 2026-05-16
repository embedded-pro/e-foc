#include "cucumber_cpp/Steps.hpp"
#include "hal/interfaces/Can.hpp"
#include "integration_tests/hardware_in_the_loop/support/HilFixture.hpp"
#include "integration_tests/hardware_in_the_loop/support/Timeouts.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>

using namespace hil;

namespace
{
    constexpr uint32_t expectedCanId{ 256 };
    constexpr std::array<uint8_t, 3> expectedCanPayload{ 0x01, 0x02, 0x03 };
}

WHEN(R"(a CAN command is transmitted to the hardware target)")
{
    auto& fixture = context.Get<HilFixture>();

    fixture.StartCanCapture();

    ASSERT_TRUE(fixture.SendCommand("can_start 500000", timeouts::command))
        << "can_start command did not receive a response";

    ASSERT_TRUE(fixture.SendCommand("can_send 256 1 2 3", timeouts::command))
        << "can_send command did not receive a response within timeout";

    fixture.MarkCanReference();

    fixture.FlushPartialLines(timeouts::command);

    const auto& lines = fixture.allLines;
    const bool acknowledged = std::any_of(lines.begin(), lines.end(),
        [](const std::string& l)
        {
            return l.find("CAN frame sent") != std::string::npos;
        });
    ASSERT_TRUE(acknowledged) << "CLI did not acknowledge can_send";
}

THEN(R"(a CAN response frame shall be received within 10 milliseconds)")
{
    auto& fixture = context.Get<HilFixture>();

    hal::Can::Message payload;
    std::chrono::milliseconds elapsed{ 0 };
    const auto expectedId = hal::Can::Id::Create11BitId(expectedCanId);

    ASSERT_TRUE(fixture.WaitForCanFrame(expectedId, payload,
        std::chrono::milliseconds{ 10 }, elapsed))
        << "No CAN frame with id 0x" << std::hex << expectedCanId
        << " observed on the bus within 10 ms";

    ASSERT_EQ(payload.size(), expectedCanPayload.size())
        << "CAN frame payload length mismatch";
    for (std::size_t i = 0; i < expectedCanPayload.size(); ++i)
        EXPECT_EQ(payload[i], expectedCanPayload[i])
            << "CAN payload byte " << i << " mismatch";

    EXPECT_LE(elapsed.count(), 10)
        << "CAN response latency " << elapsed.count()
        << " ms exceeds the 10 ms budget (REQ-HIL-006)";
}
