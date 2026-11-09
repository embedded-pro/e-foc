#include "core/foc/model/SimulatedEncoder.hpp"

namespace foc
{
    SimulatedEncoder::SimulatedEncoder(MechanicalPlant& shaft)
        : shaft(shaft)
    {}

    foc::Radians SimulatedEncoder::Read()
    {
        return model.Read(shaft.Angle());
    }

    void SimulatedEncoder::Set(foc::Radians value)
    {
        if (model.HasIndex())
            model.Set(value, shaft.Angle());
        else
            shaft.Set(value);
    }

    void SimulatedEncoder::SetZero()
    {
        if (model.HasIndex())
            model.SetZero(shaft.Angle());
        else
            shaft.SetZero();
    }

    void SimulatedEncoder::SetNoise(const EncoderNoiseConfig& config)
    {
        model.SetNoise(config);
    }

    void SimulatedEncoder::SetResolution(uint32_t countsPerRevolution)
    {
        model.SetResolution(countsPerRevolution);
    }

    void SimulatedEncoder::Seed(uint32_t seed)
    {
        model.Seed(seed);
    }

    void SimulatedEncoder::SetStuck(bool stuck)
    {
        model.SetStuck(stuck, shaft.Angle());
    }

    void SimulatedEncoder::SetIndex(std::optional<foc::Radians> angle, uint32_t widthCounts)
    {
        model.SetIndex(angle, widthCounts);
    }

    bool SimulatedEncoder::HasIndex() const
    {
        return model.HasIndex();
    }

    bool SimulatedEncoder::IndexAsserted() const
    {
        return model.IndexAsserted(shaft.Angle());
    }

    void SimulatedEncoder::Sample()
    {
        model.Sample();
    }
}
