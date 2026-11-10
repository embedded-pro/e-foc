#pragma once

#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace virtual_motor
{
    struct StartOptions
    {
        std::string bus{ "default" };
        uint8_t node{ 0 };
        std::string plant{ "teknic" };
        std::string memoryFile;
        std::optional<uint32_t> seed;
        uint16_t hubPort{ services::loopback::virtualCanHubPort };
        std::optional<uint16_t> plantPort;
        bool factoryFresh{ false };
        bool hostBus{ false };
        bool restarted{ false };
    };

    struct ParsedOptions
    {
        std::optional<StartOptions> options;
        std::string error;
        bool help{ false };
    };

    ParsedOptions ParseStartOptions(const std::vector<std::string>& arguments);
    std::string StartOptionsUsage();
}
