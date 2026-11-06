#pragma once

#include "infra/util/ByteRange.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    struct LearnedModelData
    {
        uint32_t commissioning{ 0 };
        std::optional<float> inductance;
        std::optional<float> inertia;
        std::optional<float> viscousFriction;

        bool operator==(const LearnedModelData&) const = default;
    };

    static constexpr uint32_t LearnedModelMagic = 0x1EA4ED0D;
    static constexpr uint8_t LearnedModelLayoutVersion = 1;
    static constexpr std::size_t learnedModelImageSize{ 26 };

    void SerializeLearnedModel(const LearnedModelData& model, infra::ByteRange image);
    std::optional<LearnedModelData> DeserializeLearnedModel(infra::ConstByteRange image);
}
