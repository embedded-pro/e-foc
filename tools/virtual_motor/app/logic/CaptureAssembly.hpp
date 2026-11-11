#pragma once

#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace virtual_motor::app
{
    enum class CaptureState : uint8_t
    {
        idle,
        armed,
        triggered,
        receiving,
        complete,
        cancelled,
    };

    struct Capture
    {
        uint16_t number{ 0 };
        uint16_t preTrigger{ 0 };
        uint64_t firstPeriod{ 0 };
        std::vector<port::Sample> periods;
    };

    class CaptureAssembly
    {
    public:
        void Event(const port::CaptureEvent& event);
        bool Chunk(const port::CaptureChunk& chunk);
        void Interrupted();

        CaptureState State() const;
        bool InFlight() const;
        std::size_t Received() const;
        std::size_t Expected() const;
        const std::optional<Capture>& Last() const;
        std::string StatusText() const;

    private:
        void Start(const port::CaptureChunk& chunk);

        CaptureState state{ CaptureState::idle };
        uint16_t current{ 0 };
        Capture incoming;
        std::vector<bool> placed;
        std::size_t received{ 0 };
        std::optional<Capture> last;
    };
}
