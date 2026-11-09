#pragma once

#include "core/foc/model/EncoderModel.hpp"
#include "core/foc/model/MechanicalPlant.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include <cstdint>
#include <optional>

namespace foc
{
    class SimulatedEncoder
        : public drivers::Encoder
    {
    public:
        explicit SimulatedEncoder(MechanicalPlant& shaft);

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

        void SetNoise(const EncoderNoiseConfig& config);
        void SetResolution(uint32_t countsPerRevolution);
        void Seed(uint32_t seed);
        void SetStuck(bool stuck);
        void SetIndex(std::optional<foc::Radians> angle, uint32_t widthCounts);
        bool HasIndex() const;
        bool IndexAsserted() const;
        void Sample();

    private:
        MechanicalPlant& shaft;
        EncoderModel model;
    };
}
