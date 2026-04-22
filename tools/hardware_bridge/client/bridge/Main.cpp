#include "args.hxx"
#include "services/network_instantiations/NetworkAdapter.hpp"
#include "tools/hardware_bridge/client/can/TcpClientCanbus.hpp"
#include "tools/hardware_bridge/client/common/BridgeException.hpp"
#include "tools/hardware_bridge/client/common/ParseIPv4.hpp"
#include "tools/hardware_bridge/client/common/TcpClientBase.hpp"
#include "tools/hardware_bridge/client/serial/TcpClientSerial.hpp"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

namespace
{
    class ConnectionTracker
        : public tool::TcpClientObserver
    {
    public:
        explicit ConnectionTracker(tool::TcpClient& subject)
            : tool::TcpClientObserver(subject)
        {}

        void Connected() override
        {
            connectedOnce = true;
            alive = true;
        }

        void Disconnected() override
        {
            alive = false;
        }

        bool Failed() const
        {
            return !alive && !connectedOnce;
        }

        bool Dropped() const
        {
            return connectedOnce && !alive;
        }

        bool Alive() const
        {
            return alive;
        }

    private:
        bool connectedOnce{ false };
        bool alive{ false };
    };
}

int main(int argc, char* argv[])
{
    main_::NetworkAdapter network;

    args::ArgumentParser parser("bridge_client_headless -- TCP CAN+Serial bridge client.");
    args::HelpFlag help(parser, "help", "display this help menu", { 'h', "help" });
    args::ValueFlag hostArg(parser, "ip", "server host", { "host" }, std::string("host.docker.internal"));
    args::ValueFlag canPortArg(parser, "can-port", "CAN server TCP port", { "can-port" }, 5001);
    args::ValueFlag serialPortArg(parser, "serial-port", "Serial server TCP port", { "serial-port" }, 5000);

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

    const int canPortInt = args::get(canPortArg);
    const int serialPortInt = args::get(serialPortArg);

    if (canPortInt <= 0 || canPortInt > 65535)
    {
        std::cerr << "can-port out of range: " << canPortInt << '\n';
        return 1;
    }
    if (serialPortInt <= 0 || serialPortInt > 65535)
    {
        std::cerr << "serial-port out of range: " << serialPortInt << '\n';
        return 1;
    }

    try
    {
        const std::string host = args::get(hostArg);
        const auto ipv4 = tool::ParseIPv4(host);
        const auto canPort = static_cast<uint16_t>(canPortInt);
        const auto serialPort = static_cast<uint16_t>(serialPortInt);

        std::cout << "Connecting CAN to " << host << ":" << canPort << "\n";
        std::cout << "Connecting Serial to " << host << ":" << serialPort << "\n";

        tool::TcpClientCanbus can(network.ConnectionFactory(), ipv4, canPort);
        tool::TcpClientSerial serial(network.ConnectionFactory(), ipv4, serialPort);

        ConnectionTracker canTracker(can);
        ConnectionTracker serialTracker(serial);

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

        serial.ReceiveData([](infra::ConstByteRange data)
            {
                const std::string text(reinterpret_cast<const char*>(data.begin()), data.size());
                std::cout << text;
                std::cout.flush();
            });

        network.ExecuteUntil([&]
            {
                return canTracker.Failed() || serialTracker.Failed() || canTracker.Dropped() || serialTracker.Dropped();
            });

        const bool canFailed = canTracker.Failed() || canTracker.Dropped();
        const bool serialFailed = serialTracker.Failed() || serialTracker.Dropped();

        if (canFailed || serialFailed)
        {
            std::cerr << "Bridge stopped: CAN alive=" << canTracker.Alive()
                      << ", Serial alive=" << serialTracker.Alive() << "\n";
            return 1;
        }
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
