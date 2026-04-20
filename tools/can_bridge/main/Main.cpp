#include "services/network_instantiations/NetworkAdapter.hpp"
#include "tools/can_bridge/lib/TcpClientCanbus.hpp"
#include <cstdlib>
#include <cstdio>

static main_::NetworkAdapter network;

namespace
{
    const char* GetEnvOrDefault(const char* name, const char* defaultValue)
    {
        const char* value = std::getenv(name);
        return value != nullptr ? value : defaultValue;
    }
}

int main(int argc, const char* argv[])
{
    std::printf("CAN TCP Bridge Client\n");

    const char* hostString = argc > 1 ? argv[1] : GetEnvOrDefault("CAN_BRIDGE_HOST", "172.17.0.1");
    const char* portString = argc > 2 ? argv[2] : GetEnvOrDefault("CAN_BRIDGE_PORT", "5001");

    unsigned int host0 = 172;
    unsigned int host1 = 17;
    unsigned int host2 = 0;
    unsigned int host3 = 1;
    if (std::sscanf(hostString, "%u.%u.%u.%u", &host0, &host1, &host2, &host3) != 4
        || host0 > 255
        || host1 > 255
        || host2 > 255
        || host3 > 255)
    {
        host0 = 172;
        host1 = 17;
        host2 = 0;
        host3 = 1;
    }

    long portValue = std::strtol(portString, nullptr, 10);
    if (portValue <= 0 || portValue > 65535)
        portValue = 5001;

    const services::IPv4Address serverAddress{
        static_cast<uint8_t>(host0),
        static_cast<uint8_t>(host1),
        static_cast<uint8_t>(host2),
        static_cast<uint8_t>(host3)
    };
    const uint16_t serverPort = static_cast<uint16_t>(portValue);

    std::printf("Connecting to %u.%u.%u.%u:%u\n", host0, host1, host2, host3, serverPort);

    tool::TcpClientCanbus can(network.ConnectionFactory(), serverAddress, serverPort);

    can.ReceiveData([](hal::Can::Id id, const hal::Can::Message& data)
        {
            if (id.Is29BitId())
                std::printf("CAN RX [0x%08x] DLC=%zu:", id.Get29BitId(), data.size());
            else
                std::printf("CAN RX [0x%03x] DLC=%zu:", id.Get11BitId(), data.size());

            for (auto byte : data)
                std::printf(" %02x", byte);
            std::printf("\n");
        });

    hal::Can::Message testMsg;
    testMsg.push_back(0xDE);
    testMsg.push_back(0xAD);
    testMsg.push_back(0xBE);
    testMsg.push_back(0xEF);

    auto testId = hal::Can::Id::Create29BitId(0x100);
    can.SendData(testId, testMsg, [](bool success)
        {
            std::printf("CAN TX %s\n", success ? "ok" : "failed");
        });

    network.Run();

    return 0;
}
