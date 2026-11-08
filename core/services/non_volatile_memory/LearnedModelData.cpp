#include "core/services/non_volatile_memory/LearnedModelData.hpp"
#include "core/services/non_volatile_memory/NvmBytes.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <array>
#include <bit>
#include <cstddef>

namespace services
{
    namespace
    {
        constexpr std::size_t versionOffset{ 4 };
        constexpr std::size_t commissioningOffset{ 5 };
        constexpr std::size_t quantitiesOffset{ 9 };
        constexpr std::size_t presentOffset{ 21 };
        constexpr std::size_t crcOffset{ 22 };

        std::byte PresenceBit(std::size_t quantity)
        {
            return std::byte{ 1 } << quantity;
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

        nvm::Put<uint32_t>(image, 0, LearnedModelMagic);
        image[versionOffset] = LearnedModelLayoutVersion;
        nvm::Put<uint32_t>(image, commissioningOffset, model.commissioning);

        std::byte present{ 0 };
        const auto quantities = QuantitiesOf(model);
        for (std::size_t quantity = 0; quantity != quantities.size(); ++quantity)
        {
            nvm::Put<uint32_t>(image, quantitiesOffset + quantity * sizeof(float), std::bit_cast<uint32_t>(quantities[quantity]->value_or(0.0f)));
            if (quantities[quantity]->has_value())
                present |= PresenceBit(quantity);
        }

        image[presentOffset] = std::to_integer<uint8_t>(present);
        nvm::Put<uint32_t>(image, crcOffset, nvm::Crc32(infra::Head(infra::ConstByteRange{ image }, crcOffset)));
    }

    std::optional<LearnedModelData> DeserializeLearnedModel(infra::ConstByteRange image)
    {
        really_assert(image.size() == learnedModelImageSize);

        if (nvm::Get<uint32_t>(image, 0) != LearnedModelMagic || image[versionOffset] != LearnedModelLayoutVersion || nvm::Get<uint32_t>(image, crcOffset) != nvm::Crc32(infra::Head(image, crcOffset)))
            return std::nullopt;

        LearnedModelData model;
        model.commissioning = nvm::Get<uint32_t>(image, commissioningOffset);
        const auto quantities = QuantitiesOf(model);
        const auto present = std::byte{ image[presentOffset] };
        for (std::size_t quantity = 0; quantity != quantities.size(); ++quantity)
            if ((present & PresenceBit(quantity)) != std::byte{ 0 })
                *quantities[quantity] = std::bit_cast<float>(nvm::Get<uint32_t>(image, quantitiesOffset + quantity * sizeof(float)));

        return model;
    }
}
