#include "tools/virtual_motor/app/logic/MotorSpec.hpp"
#include <format>

namespace virtual_motor::app
{
    std::vector<std::string> EngineArguments(const MotorSpec& spec, const EnginePorts& ports, bool factoryFresh)
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
        if (factoryFresh)
            arguments.emplace_back("--factory-fresh");
        return arguments;
    }

    std::string MotorFolderName(const MotorSpec& spec)
    {
        return std::format("{}-{}", spec.bus, static_cast<uint32_t>(spec.node));
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
