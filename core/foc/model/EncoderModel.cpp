#include "core/foc/model/EncoderModel.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include <cmath>
#include <numbers>

namespace foc
{
    void EncoderModel::SetNoise(const EncoderNoiseConfig& newConfig)
    {
        config = newConfig;
        lastSample = 0.0f;
    }

    void EncoderModel::SetResolution(uint32_t counts)
    {
        countsPerRevolution = counts;
    }

    void EncoderModel::Seed(uint32_t seed)
    {
        engine.seed(seed);
    }

    void EncoderModel::SetStuck(bool nowStuck, foc::Radians rotorAngle)
    {
        if (nowStuck && !stuck)
            stuckAngle = rotorAngle;

        stuck = nowStuck;
    }

    void EncoderModel::Sample()
    {
        lastSample = config.sigmaRadians * distribution(engine);
    }

    foc::Radians EncoderModel::Read(foc::Radians rotorAngle) const
    {
        const auto angle = stuck ? stuckAngle : rotorAngle;
        float measured = angle.Value() + config.biasRadians + lastSample;

        if (countsPerRevolution != 0)
        {
            const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(countsPerRevolution);
            measured = std::floor(measured / step) * step;
        }

        return foc::Radians{ detail::PositionWithWrapAround(measured) };
    }
}
