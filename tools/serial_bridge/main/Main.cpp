#include "services/network_instantiations/NetworkAdapter.hpp"
#include "tools/serial_bridge/lib/TcpClientSerial.hpp"
#include <cstdio>

static main_::NetworkAdapter network;

int main()
{
    std::printf("Serial TCP Bridge Client\n");

    tool::TcpClientSerial serial(network.ConnectionFactory(), services::IPv4Address{ 172, 17, 0, 1 }, 5000);

    serial.ReceiveData([](infra::ConstByteRange data)
        {
            std::printf("RX [%zu]: ", data.size());
            for (auto byte : data)
                std::printf("%02x ", byte);
            std::printf("\n");
        });

    static const uint8_t hello[] = { 'H', 'e', 'l', 'l', 'o', '\r', '\n' };
    serial.SendData(infra::MakeByteRange(hello), []()
        {
            std::printf("TX complete\n");
        });

    network.Run();

    return 0;
}
