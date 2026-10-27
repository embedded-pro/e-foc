#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/model/CurrentSense.hpp"
#include "core/foc/model/PowerStage.hpp"
#include <cstdint>
#include <random>

namespace foc
{
    struct CurrentNoiseConfig
    {
        float sigmaAmpere{ 0.0f };
        float biasAmpereA{ 0.0f };
        float biasAmpereB{ 0.0f };
        float biasAmpereC{ 0.0f };
    };

    class PhaseCurrentSensor
    {
    public:
        void SetNoise(const CurrentNoiseConfig& config);
        void SetChain(const CurrentSenseConfig& config);
        void Seed(uint32_t seed);

        void Measure(const foc::PhasePwmDutyCycles& duties, const foc::PhaseCurrents& actual, const PowerStageConfig& powerStage, float period);
        foc::PhaseCurrents Last() const;

    private:
        float SampleNoise();

    private:
        CurrentNoiseConfig noise{};
        CurrentSenseConfig chain{};
        std::mt19937 engine{ std::random_device{}() };
        std::normal_distribution<float> distribution{ 0.0f, 1.0f };
        foc::PhaseCurrents last{};
    };
}
