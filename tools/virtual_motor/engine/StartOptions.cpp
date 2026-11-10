#include "tools/virtual_motor/engine/StartOptions.hpp"
#include "can-lite/drivers/codec/SlcanCommandDecoder.hpp"
#include "numerical/math/Math.hpp"
#include <algorithm>
#include <charconv>
#include <format>
#include <functional>
#include <map>
#include <string_view>

namespace virtual_motor
{
    namespace
    {
        template<class Integer>
        std::optional<Integer> Number(std::string_view text, Integer minimum, Integer maximum)
        {
            uint64_t value{ 0 };
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || value < minimum || value > maximum)
                return std::nullopt;
            return static_cast<Integer>(value);
        }

        bool IsNameCharacter(char character)
        {
            return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '-' || character == '_';
        }

        bool IsBusName(std::string_view name)
        {
            return !name.empty() && name.size() <= services::virtualBusMaxNameLength && std::ranges::all_of(name, IsNameCharacter);
        }

        bool IsFileNameCharacter(char character)
        {
            return IsNameCharacter(character) || character == '.';
        }

        bool IsFileNameInWorkingDirectory(std::string_view name)
        {
            return !name.empty() && name.front() != '.' && std::ranges::all_of(name, IsFileNameCharacter);
        }

        constexpr float largestCurrentOffset{ 10.0f };

        std::optional<float> Ampere(std::string_view text)
        {
            float value{ 0.0f };
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size() || !math::IsFinite(value) || math::Abs(value) > largestCurrentOffset)
                return std::nullopt;
            return value;
        }

        std::optional<std::array<float, 3>> CurrentOffset(std::string_view text)
        {
            std::array<float, 3> offset{};
            for (std::size_t phase = 0; phase != offset.size(); ++phase)
            {
                const auto comma = text.find(',');
                const auto last = phase + 1 == offset.size();
                if (last == (comma != std::string_view::npos))
                    return std::nullopt;
                const auto value = Ampere(text.substr(0, comma));
                if (!value.has_value())
                    return std::nullopt;
                offset[phase] = *value;
                text = last ? std::string_view{} : text.substr(comma + 1);
            }
            return offset;
        }

        using ValueReader = std::function<std::string(StartOptions&, std::string_view)>;

        const std::map<std::string, ValueReader, std::less<>>& ValueOptions()
        {
            static const std::map<std::string, ValueReader, std::less<>> readers{
                { "--bus", [](StartOptions& options, std::string_view value)
                    {
                        options.bus = value;
                        return IsBusName(value) ? std::string{} : std::string{ "--bus must be 1 to 24 letters, digits, '-' or '_'" };
                    } },
                { "--node", [](StartOptions& options, std::string_view value)
                    {
                        const auto node = Number<uint8_t>(value, 0, 255);
                        options.node = node.value_or(0);
                        return node.has_value() ? std::string{} : std::string{ "--node must be a number from 0 to 255" };
                    } },
                { "--plant", [](StartOptions& options, std::string_view value)
                    {
                        options.plant = value;
                        return std::string{};
                    } },
                { "--memory", [](StartOptions& options, std::string_view value)
                    {
                        if (!IsFileNameInWorkingDirectory(value))
                            return std::string{ "--memory must name a file in the working directory: letters, digits, '.', '-' and '_', not starting with '.'" };
                        options.memoryFile = value;
                        return std::string{};
                    } },
                { "--seed", [](StartOptions& options, std::string_view value)
                    {
                        options.seed = Number<uint32_t>(value, 0, UINT32_MAX);
                        return options.seed.has_value() ? std::string{} : std::string{ "--seed must be a number from 0 to 4294967295" };
                    } },
                { "--current-offset", [](StartOptions& options, std::string_view value)
                    {
                        options.currentOffset = CurrentOffset(value);
                        return options.currentOffset.has_value() ? std::string{} : std::string{ "--current-offset must be three currents from -10 to 10 A, as a,b,c" };
                    } },
                { "--hub-port", [](StartOptions& options, std::string_view value)
                    {
                        const auto port = Number<uint16_t>(value, 1, UINT16_MAX);
                        options.hubPort = port.value_or(0);
                        return port.has_value() ? std::string{} : std::string{ "--hub-port must be a number from 1 to 65535" };
                    } },
                { "--plant-port", [](StartOptions& options, std::string_view value)
                    {
                        options.plantPort = Number<uint16_t>(value, 1, UINT16_MAX);
                        return options.plantPort.has_value() ? std::string{} : std::string{ "--plant-port must be a number from 1 to 65535" };
                    } },
            };
            return readers;
        }

        bool ReadFlag(StartOptions& options, std::string_view argument)
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
            const auto& argument = arguments[index++];
            if (ReadFlag(options, argument))
                return {};

            const auto reader = ValueOptions().find(argument);
            if (reader == ValueOptions().end())
                return std::format("unknown option {}", argument);
            if (index == arguments.size())
                return std::format("{} needs a value", argument);

            return reader->second(options, arguments[index++]);
        }
    }

    ParsedOptions ParseStartOptions(const std::vector<std::string>& arguments)
    {
        if (std::ranges::find(arguments, "--help") != arguments.end())
            return { std::nullopt, {}, true };

        StartOptions options;
        std::size_t index{ 0 };
        while (index != arguments.size())
            if (auto error = Read(options, arguments, index); !error.empty())
                return { std::nullopt, error, false };

        if (options.memoryFile.empty())
            options.memoryFile = std::format("virtual-motor-{}-{}.nvm", options.bus, static_cast<uint32_t>(options.node));

        return { options, {}, false };
    }

    std::string StartOptionsUsage()
    {
        return "Usage: e-foc-virtual-motor [options]\n"
               "Runs the e-foc application against a simulated motor as one node on a virtual CAN bus.\n\n"
               "  --bus <name>        virtual CAN bus to join: letters, digits, '-' and '_' (default: default)\n"
               "  --node <0-255>      board ID the CAN node address is built from (default: 0)\n"
               "  --plant <preset>    teknic, anaheim, bench or another preset, or a plant description file (default: teknic)\n"
               "  --memory <file>     file in the working directory holding the non-volatile memory (default: virtual-motor-<bus>-<node>.nvm)\n"
               "  --factory-fresh     start with erased memory, as a new board\n"
               "  --seed <n>          seed of every noise source\n"
               "  --current-offset <a>,<b>,<c>\n"
               "                      offset in A the current-sense chain adds to each measured phase current\n"
               "  --host-bus          host the virtual CAN hub when none is listening\n"
               "  --hub-port <n>      loopback port of the hub (default: 29536)\n"
               "  --plant-port <n>    loopback port of the Virtual Motor app's plant port to connect to\n"
               "  --help              show this text\n";
    }
}
