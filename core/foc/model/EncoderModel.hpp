#pragma once

#include "core/foc/interfaces/Units.hpp"
#include <cstdint>
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

        void Sample();
        foc::Radians Read(foc::Radians rotorAngle) const;

    private:
        EncoderNoiseConfig config{};
        std::mt19937 engine{ std::random_device{}() };
        std::normal_distribution<float> distribution{ 0.0f, 1.0f };
        float lastSample{ 0.0f };
        uint32_t countsPerRevolution{ 0 };
        bool stuck{ false };
        foc::Radians stuckAngle{ 0.0f };
    };
}
