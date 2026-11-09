#pragma once

#include "core/services/mechanical_system_ident/EncoderResolution.hpp"
#include <cstdint>
#include <optional>

namespace services
{
    struct ClosedWindow
    {
        float speed{ 0.0f };
        float acceleration{ 0.0f };
        float centredSpeed{ 0.0f };
        float centredTorque{ 0.0f };
        bool hadMean{ false };
        bool hadSpeed{ false };
        bool resolutionKnown{ false };
        uint32_t sample{ 0 };
        uint32_t windowSamples{ 1 };
    };

    // Runs in the sampling interrupt: merges samples into windows and centres speed and torque on one instant.
    class SampleWindow
    {
    public:
        void Restart(float position, float samplingPeriod);
        std::optional<ClosedWindow> Add(float position, float torque);

    private:
        struct TorqueKernel
        {
            float sum{ 0.0f };
            float rise{ 0.0f };
            float peak{ 0.0f };
            float fall{ 0.0f };
        };

        void Accumulate(float position, float torque);
        ClosedWindow Close(float position);
        bool Resize();

        EncoderResolution resolution;
        float samplingPeriod{ 1.0f };
        std::size_t windowSamples{ 1 };
        std::size_t samplesInWindow{ 0 };
        float windowPeriod{ 1.0f };
        std::size_t samplesSinceStart{ 0 };
        float previousPosition{ 0.0f };
        float previousSpeed{ 0.0f };
        bool hasPreviousMean{ false };
        bool hasPreviousSpeed{ false };
        float positionSum{ 0.0f };
        float previousMeanToEnd{ 0.0f };
        TorqueKernel torque;
        float earlierTorqueRise{ 0.0f };
        float previousTorqueRise{ 0.0f };
        float previousTorquePeak{ 0.0f };
    };
}
