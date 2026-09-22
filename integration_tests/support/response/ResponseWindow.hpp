#pragma once

#include "integration_tests/support/response/PlantTrace.hpp"
#include <cstddef>
#include <optional>
#include <vector>

namespace integration::response
{
    enum class Signal
    {
        speed,
        position,
        currentQ
    };

    constexpr std::size_t speedWindowSamples = 500;
    constexpr std::size_t positionWindowSamples = 500;
    constexpr std::size_t currentWindowSamples = 256;
    constexpr std::size_t disturbanceWindowSamples = 400;

    const char* SignalName(Signal signal);
    const char* SignalUnit(Signal signal);
    std::optional<Signal> ParseSignal(const std::string& word);
    std::size_t StepWindowSamples(Signal signal);

    float SampleValue(const PlantSample& sample, Signal signal);

    std::vector<float> Extract(const PlantTrace& trace, Signal signal, uint32_t fromTick, std::size_t count);
    std::optional<float> ValueAt(const PlantTrace& trace, Signal signal, uint32_t tick);
    std::vector<float> NormaliseStep(const std::vector<float>& raw, float initial, float reference);
}
