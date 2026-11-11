#pragma once

#include "tools/virtual_motor/port/PlantPortMessages.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace virtual_motor::app
{
    struct Envelope
    {
        uint64_t firstPeriod{ 0 };
        uint8_t periods{ 0 };
        bool afterGap{ false };
        port::Sample minimum{};
        port::Sample maximum{};
        port::Sample last{};
    };

    class TraceHistory
    {
    public:
        explicit TraceHistory(std::size_t capacity);

        void Add(const port::Window& window);
        void Clear();

        std::size_t Count() const;
        const Envelope& At(std::size_t index) const;
        uint32_t Dropped() const;
        uint64_t Revision() const;

    private:
        std::size_t capacity;
        std::vector<Envelope> windows;
        std::size_t oldest{ 0 };
        std::optional<uint32_t> lastSequence;
        uint32_t dropped{ 0 };
        uint64_t revision{ 0 };
    };
}
