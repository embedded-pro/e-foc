#include "tools/virtual_motor/app/logic/MotorSpec.hpp"
#include <algorithm>
#include <format>

namespace virtual_motor::app
{
    std::vector<std::string> EngineArguments(const MotorSpec& spec, const EnginePorts& ports, StartMode start)
    {
        std::vector<std::string> arguments{
            "--bus",
            spec.bus,
            "--node",
            std::to_string(spec.node),
            "--plant",
            spec.plant,
            "--plant-port",
            std::to_string(ports.plantPort),
            "--hub-port",
            std::to_string(ports.hubPort),
        };
        if (start == StartMode::factoryFresh)
            arguments.emplace_back("--factory-fresh");
        else if (start == StartMode::calibrated)
            arguments.emplace_back("--calibrated");
        if (std::ranges::any_of(spec.currentOffset, [](float offset)
                {
                    return offset != 0.0f;
                }))
        {
            arguments.emplace_back("--current-offset");
            arguments.push_back(std::format("{},{},{}", spec.currentOffset[0], spec.currentOffset[1], spec.currentOffset[2]));
        }
        return arguments;
    }

    std::string MotorFolderName(const MotorSpec& spec)
    {
        return std::format("{}-{}", spec.bus, static_cast<uint32_t>(spec.node));
    }

    std::string CurrentOffsetText(const std::array<float, 3>& offset)
    {
        return std::format("a {:.4g} A · b {:.4g} A · c {:.4g} A", offset[0], offset[1], offset[2]);
    }

    std::string StateText(MotorState state)
    {
        switch (state)
        {
            case MotorState::starting:
                return "Starting…";
            case MotorState::running:
                return "Running";
            case MotorState::paused:
                return "Paused";
            case MotorState::restarting:
                return "Restarting…";
            case MotorState::stopped:
                return "Stopped";
            default:
                return "Crashed";
        }
    }
}
