#include "core/services/non_volatile_memory/LearnedModelData.hpp"
#include "infra/util/Crc.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <array>
#include <bit>

namespace services
{
    namespace
    {
        constexpr std::size_t versionOffset{ 4 };
        constexpr std::size_t commissioningOffset{ 5 };
        constexpr std::size_t quantitiesOffset{ 9 };
        constexpr std::size_t presentOffset{ 21 };
        constexpr std::size_t crcOffset{ 22 };

        void Put(infra::ByteRange image, std::size_t offset, uint32_t value)
        {
            for (std::size_t byte = 0; byte != sizeof(uint32_t); ++byte)
                image[offset + byte] = static_cast<uint8_t>(value >> (8 * byte));
        }

        uint32_t Get(infra::ConstByteRange image, std::size_t offset)
        {
            uint32_t value{ 0 };
            for (std::size_t byte = 0; byte != sizeof(uint32_t); ++byte)
                value |= static_cast<uint32_t>(image[offset + byte]) << (8 * byte);
            return value;
        }

        uint32_t Crc(infra::ConstByteRange bytes)
        {
            infra::Crc32 crc;
            crc.Update(bytes);
            return crc.Result();
        }

        std::array<const std::optional<float>*, 3> QuantitiesOf(const LearnedModelData& model)
        {
            return { &model.inductance, &model.inertia, &model.viscousFriction };
        }

        std::array<std::optional<float>*, 3> QuantitiesOf(LearnedModelData& model)
        {
            return { &model.inductance, &model.inertia, &model.viscousFriction };
        }
    }

    void SerializeLearnedModel(const LearnedModelData& model, infra::ByteRange image)
    {
        really_assert(image.size() == learnedModelImageSize);

        Put(image, 0, LearnedModelMagic);
        image[versionOffset] = LearnedModelLayoutVersion;
        Put(image, commissioningOffset, model.commissioning);

        uint8_t present{ 0 };
        const auto quantities = QuantitiesOf(model);
        for (std::size_t quantity = 0; quantity != quantities.size(); ++quantity)
        {
            Put(image, quantitiesOffset + quantity * sizeof(float), std::bit_cast<uint32_t>(quantities[quantity]->value_or(0.0f)));
            if (quantities[quantity]->has_value())
                present |= static_cast<uint8_t>(1u << quantity);
        }

        image[presentOffset] = present;
        Put(image, crcOffset, Crc(infra::Head(infra::ConstByteRange{ image }, crcOffset)));
    }

    std::optional<LearnedModelData> DeserializeLearnedModel(infra::ConstByteRange image)
    {
        really_assert(image.size() == learnedModelImageSize);

        if (Get(image, 0) != LearnedModelMagic || image[versionOffset] != LearnedModelLayoutVersion || Get(image, crcOffset) != Crc(infra::Head(image, crcOffset)))
            return std::nullopt;

        LearnedModelData model;
        model.commissioning = Get(image, commissioningOffset);
        const auto quantities = QuantitiesOf(model);
        for (std::size_t quantity = 0; quantity != quantities.size(); ++quantity)
            if ((image[presentOffset] & (1u << quantity)) != 0)
                *quantities[quantity] = std::bit_cast<float>(Get(image, quantitiesOffset + quantity * sizeof(float)));

        return model;
    }
}
