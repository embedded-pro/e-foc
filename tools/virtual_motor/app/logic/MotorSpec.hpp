#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace virtual_motor::app
{
    enum class StartMode : uint8_t
    {
        keepMemory,
        factoryFresh,
    };

    struct MotorSpec
    {
        std::string bus{ "default" };
        uint8_t node{ 0 };
        std::string plant{ "teknic" };
        StartMode start{ StartMode::keepMemory };
    };

    enum class MotorState : uint8_t
    {
        starting,
        running,
        paused,
        restarting,
        stopped,
        crashed,
    };

    struct EnginePorts
    {
        uint16_t plantPort{ 0 };
        uint16_t hubPort{ 0 };
    };

    std::vector<std::string> EngineArguments(const MotorSpec& spec, const EnginePorts& ports, bool factoryFresh);
    std::string MotorFolderName(const MotorSpec& spec);
    std::string StateText(MotorState state);
}
