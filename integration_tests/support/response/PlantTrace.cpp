#include "integration_tests/support/response/PlantTrace.hpp"
#include <numbers>
#include <sstream>

namespace
{
    constexpr float milli = 1.0e-3f;
    constexpr float micro = 1.0e-6f;
    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;

    bool StartsWith(const std::string& line, const char* prefix, std::string& rest)
    {
        const std::string full = std::string{ prefix } + " ";
        if (line.rfind(full, 0) != 0)
            return false;
        rest = line.substr(full.size());
        return true;
    }

    std::optional<uint8_t> HexPair(const std::string& text, std::size_t at)
    {
        auto digit = [](char c) -> int
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        };

        if (at + 1 >= text.size())
            return std::nullopt;
        const int hi = digit(text[at]);
        const int lo = digit(text[at + 1]);
        if (hi < 0 || lo < 0)
            return std::nullopt;
        return static_cast<uint8_t>((hi << 4) | lo);
    }
}

namespace integration::response
{
    bool PlantTrace::Consume(const std::string& line)
    {
        std::string rest;
        if (StartsWith(line, "PLANT", rest))
            return ConsumeSample(rest);
        if (StartsWith(line, "PLANT_START", rest))
            return ConsumeStart(rest);
        if (StartsWith(line, "PLANT_EVENT", rest))
            return ConsumeEvent(rest);
        if (StartsWith(line, "PLANT_STOP", rest))
            return ConsumeStop(rest);
        if (StartsWith(line, "CAN_RX_AT", rest))
            return ConsumeCommand(rest);
        return false;
    }

    void PlantTrace::Clear()
    {
        samples.clear();
        events.clear();
        commands.clear();
        startTick.reset();
        stopTick.reset();
        dropped = 0;
        previousWrappedTheta.reset();
        turns = 0.0f;
    }

    bool PlantTrace::ConsumeSample(const std::string& rest)
    {
        std::istringstream in{ rest };
        uint32_t tick{};
        long omegaMilli{};
        long thetaMicro{};
        long iqMicro{};
        long idMicro{};
        long torqueMicro{};
        if (!(in >> tick >> omegaMilli >> thetaMicro >> iqMicro >> idMicro >> torqueMicro))
            return false;

        samples.push_back(PlantSample{
            tick,
            static_cast<float>(omegaMilli) * milli,
            Unwrap(static_cast<float>(thetaMicro) * micro),
            static_cast<float>(iqMicro) * micro,
            static_cast<float>(idMicro) * micro,
            static_cast<float>(torqueMicro) * micro,
        });
        return true;
    }

    bool PlantTrace::ConsumeStart(const std::string& rest)
    {
        std::istringstream in{ rest };
        uint32_t tick{};
        if (!(in >> tick))
            return false;

        samples.clear();
        events.clear();
        stopTick.reset();
        dropped = 0;
        previousWrappedTheta.reset();
        turns = 0.0f;
        startTick = tick;
        return true;
    }

    bool PlantTrace::ConsumeEvent(const std::string& rest)
    {
        std::istringstream in{ rest };
        uint32_t tick{};
        std::string kind;
        long valueMicro{};
        if (!(in >> tick >> kind >> valueMicro))
            return false;

        events.push_back(PlantEvent{ tick, kind, static_cast<float>(valueMicro) * micro });
        return true;
    }

    bool PlantTrace::ConsumeStop(const std::string& rest)
    {
        std::istringstream in{ rest };
        uint32_t tick{};
        uint32_t droppedCount{};
        if (!(in >> tick >> droppedCount))
            return false;

        stopTick = tick;
        dropped = droppedCount;
        return true;
    }

    bool PlantTrace::ConsumeCommand(const std::string& rest)
    {
        std::istringstream in{ rest };
        uint32_t tick{};
        std::string idHex;
        std::string payloadHex;
        if (!(in >> tick >> idHex))
            return false;
        in >> payloadHex;

        uint32_t rawId{};
        try
        {
            rawId = static_cast<uint32_t>(std::stoul(idHex, nullptr, 16));
        }
        catch (...)
        {
            return false;
        }

        std::vector<uint8_t> payload;
        for (std::size_t i = 0; i + 1 < payloadHex.size(); i += 2)
        {
            const auto byte = HexPair(payloadHex, i);
            if (!byte)
                break;
            payload.push_back(*byte);
        }

        commands.push_back(CommandStamp{ tick, rawId, payload });
        return true;
    }

    float PlantTrace::Unwrap(float wrappedTheta)
    {
        if (previousWrappedTheta)
        {
            const float delta = wrappedTheta - *previousWrappedTheta;
            if (delta > std::numbers::pi_v<float>)
                turns -= 1.0f;
            else if (delta < -std::numbers::pi_v<float>)
                turns += 1.0f;
        }
        previousWrappedTheta = wrappedTheta;
        return wrappedTheta + turns * twoPi;
    }

    const std::vector<PlantSample>& PlantTrace::Samples() const
    {
        return samples;
    }

    const std::vector<PlantEvent>& PlantTrace::Events() const
    {
        return events;
    }

    const std::vector<CommandStamp>& PlantTrace::Commands() const
    {
        return commands;
    }

    std::optional<uint32_t> PlantTrace::StartTick() const
    {
        return startTick;
    }

    std::optional<uint32_t> PlantTrace::StopTick() const
    {
        return stopTick;
    }

    std::optional<uint32_t> PlantTrace::LastSampleTick() const
    {
        if (samples.empty())
            return std::nullopt;
        return samples.back().tick;
    }

    std::optional<uint32_t> PlantTrace::EventTick(const std::string& kind) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->kind == kind)
                return it->tick;
        return std::nullopt;
    }

    std::optional<uint32_t> PlantTrace::CommandTick(uint32_t rawId) const
    {
        for (auto it = commands.rbegin(); it != commands.rend(); ++it)
            if (it->rawId == rawId)
                return it->tick;
        return std::nullopt;
    }

    uint32_t PlantTrace::Dropped() const
    {
        return dropped;
    }

    std::optional<uint32_t> PlantTrace::TickSpacing() const
    {
        if (samples.size() < 2)
            return std::nullopt;
        return samples[1].tick - samples[0].tick;
    }

    std::size_t PlantTrace::Gaps() const
    {
        const auto spacing = TickSpacing();
        if (!spacing)
            return 0;

        std::size_t gaps = 0;
        for (std::size_t i = 1; i < samples.size(); ++i)
            if (samples[i].tick - samples[i - 1].tick != *spacing)
                ++gaps;
        return gaps;
    }
}
