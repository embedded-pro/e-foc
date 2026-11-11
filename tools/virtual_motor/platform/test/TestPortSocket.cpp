#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include "tools/virtual_motor/platform/PortSocket.hpp"
#include "gtest/gtest.h"
#include <array>
#include <vector>

namespace
{
    namespace loopback = services::loopback;

    class TestPortSocket
        : public testing::Test
    {
    public:
        TestPortSocket()
        {
            client = loopback::Connect(loopback::LocalPort(listener).value_or(0));
            server = loopback::Accept(listener);
        }

        ~TestPortSocket() override
        {
            loopback::Close(client);
            loopback::Close(server);
            loopback::Close(listener);
        }

        intptr_t listener{ loopback::Listen(0) };
        intptr_t client{ loopback::invalidSocket };
        intptr_t server{ loopback::invalidSocket };
    };
}

TEST_F(TestPortSocket, sends_what_the_connection_takes)
{
    const std::array<uint8_t, 3> bytes{ 1, 2, 3 };

    EXPECT_EQ(virtual_motor::SendWithoutWaiting(client, infra::MakeRange(bytes)), 3u);

    std::array<uint8_t, 8> received{};
    std::optional<std::size_t> size;
    for (int attempt = 0; attempt != 1000 && size.value_or(0) == 0; ++attempt)
        size = loopback::Receive(server, infra::MakeRange(received));
    EXPECT_EQ(size, 3u);
}

TEST_F(TestPortSocket, a_full_connection_takes_nothing_instead_of_waiting)
{
    const std::vector<uint8_t> chunk(65536, 0x55);
    std::optional<std::size_t> sent{ chunk.size() };
    for (int attempt = 0; attempt != 100000 && sent.value_or(0) != 0; ++attempt)
        sent = virtual_motor::SendWithoutWaiting(client, infra::ConstByteRange{ chunk.data(), chunk.data() + chunk.size() });

    EXPECT_EQ(sent, 0u);
}

TEST_F(TestPortSocket, a_closed_connection_reports_failure)
{
    loopback::Close(server);
    server = loopback::invalidSocket;

    const std::array<uint8_t, 1> bytes{ 1 };
    std::optional<std::size_t> sent{ 1 };
    for (int attempt = 0; attempt != 1000 && sent.has_value(); ++attempt)
        sent = virtual_motor::SendWithoutWaiting(client, infra::MakeRange(bytes));

    EXPECT_FALSE(sent.has_value());
}
