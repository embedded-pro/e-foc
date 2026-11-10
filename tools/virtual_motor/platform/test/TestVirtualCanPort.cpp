#include "can-lite/drivers/hub/VirtualCanHub.hpp"
#include "can-lite/drivers/implementation/SlcanAdapter.hpp"
#include "can-lite/drivers/implementation/VirtualBusPort.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "tools/virtual_motor/platform/VirtualCanPort.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <optional>

namespace
{
    class TestVirtualCanPort
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        static uint16_t Start(services::VirtualCanHub& hub)
        {
            EXPECT_TRUE(hub.Listen(0));
            return hub.Port().value_or(0);
        }

        template<class Condition>
        bool PollUntil(Condition condition)
        {
            for (int iteration = 0; iteration != 1000 && !condition(); ++iteration)
            {
                hub.Poll();
                port.Poll();
                peer.ProcessReadEvent();
                ExecuteAllActions();
            }
            return condition();
        }

        void ConnectBoth()
        {
            ASSERT_TRUE(port.Connect("bench", 1000000));
            ASSERT_TRUE(peer.Connect("bench", 1000000));
            ASSERT_TRUE(PollUntil([this]()
                {
                    return port.IsConnected() && peer.IsConnected();
                }));
        }

        services::VirtualCanHub hub;
        uint16_t hubPort{ Start(hub) };
        virtual_motor::VirtualCanPort port{ hubPort };
        services::VirtualBusPort peerPort{ hubPort };
        services::SlcanAdapter peer{ peerPort };
    };
}

TEST_F(TestVirtualCanPort, frames_the_application_sends_reach_the_bus)
{
    ConnectBoth();
    std::optional<uint32_t> receivedId;
    peer.ReceiveData([&receivedId](hal::Can::Id id, const hal::Can::Message&)
        {
            receivedId = id.Get29BitId();
        });

    hal::Can::Message data;
    data.push_back(0x42);
    port.SendData(hal::Can::Id::Create29BitId(0x1020304), data, [](bool success)
        {
            EXPECT_TRUE(success);
        });

    PollUntil([]()
        {
            return false;
        });
    ForwardTime(std::chrono::milliseconds(1));
    ASSERT_TRUE(PollUntil([&receivedId]()
        {
            return receivedId.has_value();
        }));
    EXPECT_EQ(*receivedId, 0x1020304u);
}

TEST_F(TestVirtualCanPort, frames_on_the_bus_reach_the_application)
{
    ConnectBoth();
    std::optional<uint32_t> receivedId;
    port.ReceiveData([&receivedId](hal::Can::Id id, const hal::Can::Message&)
        {
            receivedId = id.Get29BitId();
        });

    hal::Can::Message data;
    peer.SendData(hal::Can::Id::Create29BitId(0x0abcdef), data, [](bool) {});

    PollUntil([]()
        {
            return false;
        });
    ForwardTime(std::chrono::milliseconds(1));
    ASSERT_TRUE(PollUntil([&receivedId]()
        {
            return receivedId.has_value();
        }));
    EXPECT_EQ(*receivedId, 0x0abcdefu);
}

TEST_F(TestVirtualCanPort, losing_the_hub_is_counted_as_a_bus_error)
{
    ConnectBoth();
    testing::StrictMock<testing::MockFunction<void(services::CanBusError)>> onError;
    port.SetOnError([&onError](services::CanBusError error)
        {
            onError.Call(error);
        });

    EXPECT_CALL(onError, Call(services::CanBusError::other));
    hub.Stop();
    PollUntil([this]()
        {
            return !port.IsConnected();
        });

    EXPECT_FALSE(port.IsConnected());
    EXPECT_EQ(port.ErrorStatistics().Total(), 1u);
}

TEST_F(TestVirtualCanPort, without_a_hub_the_connection_is_refused)
{
    hub.Stop();

    EXPECT_FALSE(port.Connect("bench", 1000000));
}

TEST_F(TestVirtualCanPort, a_muted_port_discards_frames_from_the_bus)
{
    ConnectBoth();
    std::optional<uint32_t> receivedId;
    port.ReceiveData([&receivedId](hal::Can::Id id, const hal::Can::Message&)
        {
            receivedId = id.Get29BitId();
        });
    port.Mute(true);

    hal::Can::Message data;
    peer.SendData(hal::Can::Id::Create29BitId(0x0abcdef), data, [](bool) {});
    PollUntil([]()
        {
            return false;
        });
    ForwardTime(std::chrono::milliseconds(1));
    PollUntil([]()
        {
            return false;
        });
    EXPECT_FALSE(receivedId.has_value());

    port.Mute(false);
    peer.SendData(hal::Can::Id::Create29BitId(0x0123456), data, [](bool) {});
    PollUntil([]()
        {
            return false;
        });
    ForwardTime(std::chrono::milliseconds(1));
    ASSERT_TRUE(PollUntil([&receivedId]()
        {
            return receivedId.has_value();
        }));
    EXPECT_EQ(*receivedId, 0x0123456u);
}

TEST_F(TestVirtualCanPort, a_muted_port_sends_nothing_but_completes_the_send)
{
    ConnectBoth();
    std::optional<uint32_t> receivedId;
    peer.ReceiveData([&receivedId](hal::Can::Id id, const hal::Can::Message&)
        {
            receivedId = id.Get29BitId();
        });
    port.Mute(true);

    bool completed{ false };
    hal::Can::Message data;
    port.SendData(hal::Can::Id::Create29BitId(0x1020304), data, [&completed](bool)
        {
            completed = true;
        });
    ForwardTime(std::chrono::milliseconds(1));
    PollUntil([]()
        {
            return false;
        });

    EXPECT_TRUE(completed);
    EXPECT_FALSE(receivedId.has_value());
}
