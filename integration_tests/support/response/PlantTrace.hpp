#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace integration::response
{
    struct PlantSample
    {
        uint32_t tick;
        float omegaMech;
        float thetaMech;
        float iq;
        float id;
        float externalTorqueNm;
    };

    struct PlantEvent
    {
        uint32_t tick;
        std::string kind;
        float value;
    };

    struct CommandStamp
    {
        uint32_t tick;
        uint32_t rawId;
        std::vector<uint8_t> payload;
    };

    class PlantTrace
    {
    public:
        bool Consume(const std::string& line);
        void Clear();

        const std::vector<PlantSample>& Samples() const;
        const std::vector<PlantEvent>& Events() const;
        const std::vector<CommandStamp>& Commands() const;

        std::optional<uint32_t> StartTick() const;
        std::optional<uint32_t> StopTick() const;
        std::optional<uint32_t> LastSampleTick() const;
        std::optional<uint32_t> EventTick(const std::string& kind) const;
        std::optional<uint32_t> CommandTick(uint32_t rawId) const;

        uint32_t Dropped() const;
        std::optional<uint32_t> TickSpacing() const;
        std::size_t Gaps() const;

    private:
        bool ConsumeSample(const std::string& rest);
        bool ConsumeStart(const std::string& rest);
        bool ConsumeEvent(const std::string& rest);
        bool ConsumeStop(const std::string& rest);
        bool ConsumeCommand(const std::string& rest);
        float Unwrap(float wrappedTheta);

    private:
        std::vector<PlantSample> samples;
        std::vector<PlantEvent> events;
        std::vector<CommandStamp> commands;
        std::optional<uint32_t> startTick;
        std::optional<uint32_t> stopTick;
        uint32_t dropped{ 0 };
        std::optional<float> previousWrappedTheta;
        float turns{ 0.0f };
    };
}
