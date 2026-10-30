#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/test/CanMock.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/can/UntracedStreamCan.hpp"
#include <gtest/gtest.h>

namespace
{
    using namespace testing;

    hal::Can::Id FocId(services::CanPriority priority, uint8_t messageType)
    {
        return hal::Can::Id::Create29BitId(services::MakeCanId(priority, can::focMotorCategoryId, messageType, 1));
    }

    class UntracedStreamCanTest
        : public Test
    {
    public:
        StrictMock<hal::CanMock> traced;
        StrictMock<hal::CanMock> untraced;
        can::UntracedStreamCan bus{ traced, untraced };
        hal::Can::Message payload;
    };
}

TEST_F(UntracedStreamCanTest, a_current_stream_frame_bypasses_the_tracing_bus)
{
    const auto id = FocId(services::CanPriority::telemetry, can::focTelemetryCurrentsId);
    EXPECT_CALL(untraced, SendData(id, _, _));

    bus.SendData(id, payload, [](bool) {});
}

TEST_F(UntracedStreamCanTest, every_other_frame_goes_through_the_tracing_bus)
{
    const auto status = FocId(services::CanPriority::telemetry, can::focTelemetryStatusResponseId);
    const auto ack = hal::Can::Id::Create29BitId(services::MakeCanId(services::CanPriority::response, services::canSystemCategoryId, can::focTelemetryCurrentsId, 1));
    const auto standard = hal::Can::Id::Create11BitId(0x123);
    EXPECT_CALL(traced, SendData(status, _, _));
    EXPECT_CALL(traced, SendData(ack, _, _));
    EXPECT_CALL(traced, SendData(standard, _, _));

    bus.SendData(status, payload, [](bool) {});
    bus.SendData(ack, payload, [](bool) {});
    bus.SendData(standard, payload, [](bool) {});
}

TEST_F(UntracedStreamCanTest, received_frames_come_through_the_tracing_bus)
{
    EXPECT_CALL(traced, ReceiveData(_));

    bus.ReceiveData([](hal::Can::Id, const hal::Can::Message&) {});
}
