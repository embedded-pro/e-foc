#include "services/network_instantiations/NetworkAdapter.hpp"
#include "tools/can_bridge/lib/TcpClientCanbus.hpp"
#include <cstdio>

static main_::NetworkAdapter network;

int main()
{
    std::printf("CAN TCP Bridge Client\n");

    tool::TcpClientCanbus can(network.ConnectionFactory(), services::IPv4Address{ 172, 17, 0, 1 }, 5001);

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
