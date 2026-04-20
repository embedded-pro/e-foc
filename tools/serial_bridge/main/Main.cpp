#include "services/network_instantiations/NetworkAdapter.hpp"
#include "tools/serial_bridge/lib/TcpClientSerial.hpp"
#include <cstdio>

static main_::NetworkAdapter network;

int main(int argc, const char* argv[])
{
    std::printf("Serial TCP Bridge Client\n");

    uint8_t address0 = 172;
    uint8_t address1 = 17;
    uint8_t address2 = 0;
    uint8_t address3 = 1;
    uint16_t port = 5000;

    if (argc > 1)
    {
        unsigned int parsed0 = 0;
        unsigned int parsed1 = 0;
        unsigned int parsed2 = 0;
        unsigned int parsed3 = 0;

        if (std::sscanf(argv[1], "%u.%u.%u.%u", &parsed0, &parsed1, &parsed2, &parsed3) == 4
            && parsed0 <= 255
            && parsed1 <= 255
            && parsed2 <= 255
            && parsed3 <= 255)
        {
            address0 = static_cast<uint8_t>(parsed0);
            address1 = static_cast<uint8_t>(parsed1);
            address2 = static_cast<uint8_t>(parsed2);
            address3 = static_cast<uint8_t>(parsed3);
        }
        else
            std::printf("Invalid IP address '%s', using default 172.17.0.1\n", argv[1]);
    }

    if (argc > 2)
    {
        unsigned int parsedPort = 0;

        if (std::sscanf(argv[2], "%u", &parsedPort) == 1 && parsedPort <= 65535)
            port = static_cast<uint16_t>(parsedPort);
        else
            std::printf("Invalid port '%s', using default 5000\n", argv[2]);
    }

    std::printf("Connecting to %u.%u.%u.%u:%u\n", address0, address1, address2, address3, port);

    tool::TcpClientSerial serial(network.ConnectionFactory(), services::IPv4Address{ address0, address1, address2, address3 }, port);

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
