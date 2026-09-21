#include "integration_tests/support/response/ResponseWindow.hpp"

namespace integration::response
{
    const char* SignalName(Signal signal)
    {
        switch (signal)
        {
            case Signal::speed:
                return "speed";
            case Signal::position:
                return "position";
            case Signal::currentQ:
                return "q-axis current";
        }
        return "?";
    }

    const char* SignalUnit(Signal signal)
    {
        switch (signal)
        {
            case Signal::speed:
                return "rad/s";
            case Signal::position:
                return "rad";
            case Signal::currentQ:
                return "A";
        }
        return "?";
    }

    std::optional<Signal> ParseSignal(const std::string& word)
    {
        if (word == "speed")
            return Signal::speed;
        if (word == "position")
            return Signal::position;
        if (word == "current" || word == "q-axis")
            return Signal::currentQ;
        return std::nullopt;
    }

    std::size_t StepWindowSamples(Signal signal)
    {
        switch (signal)
        {
            case Signal::speed:
                return speedWindowSamples;
            case Signal::position:
                return positionWindowSamples;
            case Signal::currentQ:
                return currentWindowSamples;
        }
        return 0;
    }

    float SampleValue(const PlantSample& sample, Signal signal)
    {
        switch (signal)
        {
            case Signal::speed:
                return sample.omegaMech;
            case Signal::position:
                return sample.thetaMech;
            case Signal::currentQ:
                return sample.iq;
        }
        return 0.0f;
    }

    std::vector<float> Extract(const PlantTrace& trace, Signal signal, uint32_t fromTick, std::size_t count)
    {
        std::vector<float> out;
        out.reserve(count);
        for (const auto& sample : trace.Samples())
        {
            if (sample.tick < fromTick)
                continue;
            if (out.size() == count)
                break;
            out.push_back(SampleValue(sample, signal));
        }
        return out;
    }

    std::optional<float> ValueAt(const PlantTrace& trace, Signal signal, uint32_t tick)
    {
        std::optional<float> value;
        for (const auto& sample : trace.Samples())
        {
            if (sample.tick > tick)
                break;
            value = SampleValue(sample, signal);
        }
        return value;
    }

    std::vector<float> NormaliseStep(const std::vector<float>& raw, float initial, float reference)
    {
        const float magnitude = reference - initial;
        std::vector<float> out;
        out.reserve(raw.size());
        for (const float y : raw)
            out.push_back(magnitude == 0.0f ? 0.0f : (y - initial) / magnitude);
        return out;
    }
}
