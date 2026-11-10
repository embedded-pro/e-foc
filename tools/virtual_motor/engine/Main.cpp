#include "can-lite/drivers/hub/VirtualCanHub.hpp"
#include "infra/util/StaticStorage.hpp"
#include "targets/sync_foc_sensored/main/instantiations/Logic.hpp"
#include "tools/virtual_motor/engine/StartOptions.hpp"
#include "tools/virtual_motor/platform/PlantDescription.hpp"
#include "tools/virtual_motor/platform/VirtualPlatform.hpp"
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
    std::vector<std::string> arguments;

    [[noreturn]] void Restart()
    {
        std::vector<std::string> restartArguments{ arguments };
        if (std::find(restartArguments.begin(), restartArguments.end(), "--restarted") == restartArguments.end())
            restartArguments.emplace_back("--restarted");
        std::erase(restartArguments, "--factory-fresh");

        std::vector<char*> argv;
        for (auto& argument : restartArguments)
            argv.push_back(argument.data());
        argv.push_back(nullptr);

        std::cout.flush();
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
        if (plant && options.seed)
            plant->randomSeed = *options.seed;
        return plant;
    }

    bool HostBus(services::VirtualCanHub& hub, uint16_t port)
    {
        if (hub.Listen(port))
            return true;

        std::cerr << "e-foc-virtual-motor: port " << port << " is taken; joining the hub that holds it\n";
        return false;
    }
}

int main(int argc, char* argv[])
{
    arguments.assign(argv, argv + argc);
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

    const auto& options = *parsed.options;
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

    platform.Construct(virtual_motor::BoardOptions{ options.bus, options.node, *plant, options.memoryFile, options.hubPort, options.restarted, []()
        {
            Restart();
        } });

    if (options.hostBus)
    {
        hub.Construct();
        if (HostBus(*hub, options.hubPort))
            platform->EachMillisecond([]()
                {
                    hub->Poll();
                });
    }

    logic.Construct(*platform);
    platform->Run();
}
