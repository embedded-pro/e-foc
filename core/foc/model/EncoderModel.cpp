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

    void EncoderModel::SetIndex(std::optional<foc::Radians> angle, uint32_t widthCounts)
    {
        indexAngle = angle;
        indexWidthCounts = widthCounts;
        zero = 0.0f;
    }

    bool EncoderModel::HasIndex() const
    {
        return indexAngle.has_value();
    }

    // The pulse spans the counts that follow the index angle, read off the rotor, never off a stuck reading
    bool EncoderModel::IndexAsserted(foc::Radians rotorAngle) const
    {
        if (!indexAngle)
            return false;

        const float twoPi = 2.0f * std::numbers::pi_v<float>;
        const float past = std::fmod(std::fmod(rotorAngle.Value() - indexAngle->Value(), twoPi) + twoPi, twoPi);

        return past < static_cast<float>(indexWidthCounts) * CountWidth();
    }

    void EncoderModel::Set(foc::Radians value, foc::Radians rotorAngle)
    {
        zero = detail::PositionWithWrapAround(RawReading(rotorAngle) - value.Value());
    }

    void EncoderModel::SetZero(foc::Radians rotorAngle)
    {
        zero = RawReading(rotorAngle);
    }

    float EncoderModel::CountWidth() const
    {
        constexpr uint32_t countsWithoutResolution{ 4096 };
        return 2.0f * std::numbers::pi_v<float> / static_cast<float>(countsPerRevolution != 0 ? countsPerRevolution : countsWithoutResolution);
    }

    void EncoderModel::Sample()
    {
        lastSample = config.sigmaRadians * distribution(engine);
    }

    foc::Radians EncoderModel::Read(foc::Radians rotorAngle) const
    {
        return foc::Radians{ detail::PositionWithWrapAround(RawReading(rotorAngle) - zero) };
    }

    float EncoderModel::RawReading(foc::Radians rotorAngle) const
    {
        const auto angle = stuck ? stuckAngle : rotorAngle;
        float measured = angle.Value() + config.biasRadians + lastSample;

        if (countsPerRevolution != 0)
        {
            const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(countsPerRevolution);
            measured = std::floor(measured / step) * step;
        }

        return detail::PositionWithWrapAround(measured);
    }
}
