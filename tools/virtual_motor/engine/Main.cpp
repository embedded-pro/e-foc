#include "can-lite/drivers/hub/VirtualCanHub.hpp"
#include "infra/util/StaticStorage.hpp"
#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include "tools/virtual_motor/engine/StartOptions.hpp"
#include "tools/virtual_motor/platform/PlantDescription.hpp"
#include "tools/virtual_motor/platform/PlantPortClient.hpp"
#include "tools/virtual_motor/platform/PlatformPortTarget.hpp"
#include "tools/virtual_motor/platform/VirtualPlatform.hpp"
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace
{
    [[noreturn]] void Restart(const std::vector<std::string>& arguments)
    {
        std::vector<std::string> restartArguments{ arguments };
        if (std::ranges::find(restartArguments, "--restarted") == restartArguments.end())
            restartArguments.emplace_back("--restarted");
        std::erase(restartArguments, "--factory-fresh");

        std::vector<char*> argv;
        for (auto& argument : restartArguments)
            argv.push_back(argument.data());
        argv.push_back(nullptr);

        std::cout.flush();
#if defined(__linux__)
        ::close_range(3, ~0u, 0);
#endif
#if defined(_WIN32)
        _execv(argv[0], argv.data());
#else
        execv("/proc/self/exe", argv.data());
#endif
        std::cerr << "e-foc-virtual-motor: could not restart\n";
        std::exit(EXIT_FAILURE);
    }

    std::optional<sil::SilPlantConfig> Plant(const virtual_motor::StartOptions& options)
    {
        auto plant = virtual_motor::PlantDescription(options.plant);
        if (plant && options.seed.has_value())
            plant->randomSeed = *options.seed;
        if (plant && options.currentOffset.has_value())
        {
            plant->noiseBiasAmpereA = (*options.currentOffset)[0];
            plant->noiseBiasAmpereB = (*options.currentOffset)[1];
            plant->noiseBiasAmpereC = (*options.currentOffset)[2];
        }
        return plant;
    }

    struct PlantPort
    {
        std::optional<virtual_motor::PlatformPortTarget> target;
        std::optional<virtual_motor::PlantPortClient> client;
    };

    bool HostBus(services::VirtualCanHub& hub, uint16_t port)
    {
        if (hub.Listen(port))
            return true;

        std::cerr << "e-foc-virtual-motor: port " << port << " is taken; joining the hub that holds it\n";
        return false;
    }

    bool AttachPlantPort(virtual_motor::VirtualPlatform& platform, PlantPort& plantPort, uint16_t port)
    {
        auto& client = plantPort.client.emplace(plantPort.target.emplace(platform));
        if (!client.Connect(port))
            return false;

        platform.MirrorLog([&client](infra::ConstByteRange text)
            {
                client.Log(text);
            });
        platform.EachMillisecond([&client]()
            {
                client.Millisecond();
            });
        return true;
    }

    [[nodiscard]] int Start(const virtual_motor::StartOptions& options, const std::vector<std::string>& arguments)
    {
        const auto plant = Plant(options);
        if (!plant)
        {
            std::cerr << "e-foc-virtual-motor: " << options.plant << " is neither a plant preset nor a readable plant description file\n";
            return 2;
        }

        if (options.factoryFresh)
            std::filesystem::remove(options.memoryFile);

        static infra::StaticStorage<virtual_motor::VirtualPlatform> platform;
        static infra::StaticStorage<services::VirtualCanHub> hub;
        static infra::StaticStorage<application::Logic> logic;
        static PlantPort plantPort;

        platform.Construct(virtual_motor::BoardOptions{ options.bus, options.node, *plant, options.memoryFile, options.hubPort, options.restarted, [&arguments]()
            {
                Restart(arguments);
            },
            options.plant });

        if (options.plantPort.has_value() && !AttachPlantPort(*platform, plantPort, *options.plantPort))
        {
            std::cerr << "e-foc-virtual-motor: no plant port is listening on port " << *options.plantPort << "\n";
            return EXIT_FAILURE;
        }

        hub.Construct();
        static const bool hostingBus = options.hostBus && HostBus(*hub, options.hubPort);
        platform->EachIteration([]()
            {
                if (hostingBus)
                    hub->Poll();
                if (plantPort.client)
                    plantPort.client->Poll();
            });

        logic.Construct(*platform);
        platform->Run();
        return EXIT_SUCCESS;
    }
}

int main(int argc, char* argv[])
{
#if !defined(_WIN32)
    std::signal(SIGPIPE, SIG_IGN);
#endif
    static const std::vector<std::string> arguments{ argv, argv + argc };
    const auto parsed = virtual_motor::ParseStartOptions({ arguments.begin() + 1, arguments.end() });
    if (parsed.help)
    {
        std::cout << virtual_motor::StartOptionsUsage();
        return EXIT_SUCCESS;
    }
    if (!parsed.options)
    {
        std::cerr << "e-foc-virtual-motor: " << parsed.error << "\n\n"
                  << virtual_motor::StartOptionsUsage();
        return 2;
    }

    return Start(*parsed.options, arguments);
}
