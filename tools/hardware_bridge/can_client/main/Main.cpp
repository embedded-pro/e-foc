#include "args.hxx"
#include "services/network_instantiations/NetworkAdapter.hpp"
#include "tools/hardware_bridge/can_client/lib/TcpClientCanbus.hpp"
#include "tools/hardware_bridge/common/BridgeException.hpp"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

namespace
{
    main_::NetworkAdapter network;

    services::IPv4Address ParseIPv4(const std::string& host)
    {
        unsigned int o0 = 0, o1 = 0, o2 = 0, o3 = 0;
        if (std::sscanf(host.c_str(), "%u.%u.%u.%u", &o0, &o1, &o2, &o3) != 4 || o0 > 255 || o1 > 255 || o2 > 255 || o3 > 255)
            throw tool::BridgeArgumentException("invalid IP address: " + host);
        return services::IPv4Address{
            static_cast<uint8_t>(o0),
            static_cast<uint8_t>(o1),
            static_cast<uint8_t>(o2),
            static_cast<uint8_t>(o3)
        };
    }
}

int main(int argc, char* argv[])
{
    args::ArgumentParser parser("can_bridge -- TCP CAN bridge client.");
    args::HelpFlag help(parser, "help", "display this help menu", { 'h', "help" });
    args::ValueFlag<std::string> hostArg(parser, "ip", "server host", { "host" }, std::string("host.docker.internal"));
    args::ValueFlag<int> portArg(parser, "port", "server TCP port", { "port" }, 5001);

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Help&)
    {
        std::cout << parser;
        return 0;
    }
    catch (const args::ParseError& e)
    {
        std::cerr << e.what() << '\n'
                  << parser;
        return 1;
    }

    const int port = args::get(portArg);
    if (port <= 0 || port > 65535)
    {
        std::cerr << "port out of range: " << port << '\n';
        return 1;
    }

    try
    {
        const std::string host = args::get(hostArg);
        const auto ipv4 = ParseIPv4(host);
        std::cout << "Connecting to " << host << ":" << port << "\n";

        tool::TcpClientCanbus can(network.ConnectionFactory(), ipv4, static_cast<uint16_t>(port));
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

        network.Run();
    }
    catch (const tool::BridgeArgumentException& e)
    {
        std::cerr << "Usage error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
