#pragma once

#include "e-foc-protocol/foc/Units.hpp"
#include <cstdint>
#include <optional>
#include <random>

namespace foc
{
    struct EncoderNoiseConfig
    {
        float sigmaRadians{ 0.0f };
        float biasRadians{ 0.0f };
    };

    class EncoderModel
    {
    public:
        void SetNoise(const EncoderNoiseConfig& config);
        void SetResolution(uint32_t countsPerRevolution);
        void Seed(uint32_t seed);
        void SetStuck(bool stuck, foc::Radians rotorAngle);
        // With an index the encoder also keeps its own reference: Set and SetZero move its zero, not the rotor
        void SetIndex(std::optional<foc::Radians> angle, uint32_t widthCounts);
        bool HasIndex() const;
        bool IndexAsserted(foc::Radians rotorAngle) const;

        void Sample();
        foc::Radians Read(foc::Radians rotorAngle) const;
        void Set(foc::Radians value, foc::Radians rotorAngle);
        void SetZero(foc::Radians rotorAngle);

    private:
        float CountWidth() const;
        float RawReading(foc::Radians rotorAngle) const;

        EncoderNoiseConfig config{};
        std::mt19937 engine{ std::random_device{}() };
        std::normal_distribution<float> distribution{ 0.0f, 1.0f };
        float lastSample{ 0.0f };
        uint32_t countsPerRevolution{ 0 };
        bool stuck{ false };
        foc::Radians stuckAngle{ 0.0f };
        std::optional<foc::Radians> indexAngle;
        uint32_t indexWidthCounts{ 0 };
        float zero{ 0.0f };
    };
}
