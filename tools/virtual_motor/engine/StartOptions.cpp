#include "tools/virtual_motor/engine/StartOptions.hpp"
#include "can-lite/drivers/codec/SlcanCommandDecoder.hpp"
#include <charconv>
#include <functional>
#include <map>

namespace virtual_motor
{
    namespace
    {
        template<class Integer>
        std::optional<Integer> Number(const std::string& text, Integer minimum, Integer maximum)
        {
            uint64_t value{ 0 };
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || value < minimum || value > maximum)
                return std::nullopt;
            return static_cast<Integer>(value);
        }

        using ValueReader = std::function<std::string(StartOptions&, const std::string&)>;

        const std::map<std::string, ValueReader>& ValueOptions()
        {
            static const std::map<std::string, ValueReader> readers{
                { "--bus", [](StartOptions& options, const std::string& value)
                    {
                        options.bus = value;
                        return value.empty() || value.size() > services::virtualBusMaxNameLength ? std::string{ "--bus must be 1 to 24 characters" } : std::string{};
                    } },
                { "--node", [](StartOptions& options, const std::string& value)
                    {
                        const auto node = Number<uint8_t>(value, 0, 255);
                        options.node = node.value_or(0);
                        return node ? std::string{} : std::string{ "--node must be a number from 0 to 255" };
                    } },
                { "--plant", [](StartOptions& options, const std::string& value)
                    {
                        options.plant = value;
                        return std::string{};
                    } },
                { "--memory", [](StartOptions& options, const std::string& value)
                    {
                        options.memoryFile = value;
                        return std::string{};
                    } },
                { "--seed", [](StartOptions& options, const std::string& value)
                    {
                        options.seed = Number<uint32_t>(value, 0, UINT32_MAX);
                        return options.seed ? std::string{} : std::string{ "--seed must be a number from 0 to 4294967295" };
                    } },
                { "--hub-port", [](StartOptions& options, const std::string& value)
                    {
                        const auto port = Number<uint16_t>(value, 1, UINT16_MAX);
                        options.hubPort = port.value_or(0);
                        return port ? std::string{} : std::string{ "--hub-port must be a number from 1 to 65535" };
                    } },
            };
            return readers;
        }

        bool ReadFlag(StartOptions& options, const std::string& argument)
        {
            if (argument == "--factory-fresh")
                options.factoryFresh = true;
            else if (argument == "--host-bus")
                options.hostBus = true;
            else if (argument == "--restarted")
                options.restarted = true;
            else
                return false;
            return true;
        }

        std::string Read(StartOptions& options, const std::vector<std::string>& arguments, std::size_t& index)
        {
            const auto& argument = arguments[index];
            if (ReadFlag(options, argument))
                return {};

            const auto reader = ValueOptions().find(argument);
            if (reader == ValueOptions().end())
                return "unknown option " + argument;
            if (index + 1 == arguments.size())
                return argument + " needs a value";

            return reader->second(options, arguments[++index]);
        }
    }

    ParsedOptions ParseStartOptions(const std::vector<std::string>& arguments)
    {
        StartOptions options;
        for (std::size_t index = 0; index != arguments.size(); ++index)
        {
            if (arguments[index] == "--help")
                return { std::nullopt, {}, true };

            if (auto error = Read(options, arguments, index); !error.empty())
                return { std::nullopt, error, false };
        }

        if (options.memoryFile.empty())
            options.memoryFile = "virtual-motor-" + options.bus + "-" + std::to_string(options.node) + ".nvm";

        return { options, {}, false };
    }

    std::string StartOptionsUsage()
    {
        return "Usage: e-foc-virtual-motor [options]\n"
               "Runs the e-foc application against a simulated motor as one node on a virtual CAN bus.\n\n"
               "  --bus <name>        virtual CAN bus to join (default: default)\n"
               "  --node <0-255>      board ID the CAN node address is built from (default: 0)\n"
               "  --plant <preset>    teknic, anaheim, bench or another preset, or a plant description file (default: teknic)\n"
               "  --memory <file>     file holding the non-volatile memory (default: virtual-motor-<bus>-<node>.nvm)\n"
               "  --factory-fresh     start with erased memory, as a new board\n"
               "  --seed <n>          seed of every noise source\n"
               "  --host-bus          host the virtual CAN hub when none is listening\n"
               "  --hub-port <n>      loopback port of the hub (default: 29536)\n"
               "  --help              show this text\n";
    }
}
