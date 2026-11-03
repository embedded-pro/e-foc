#include "core/services/firmware_upgrade/PackHeaderCheck.hpp"
#include <algorithm>
#include <limits>

namespace services
{
    namespace
    {
        constexpr uint16_t headerVersion{ 1 };
        constexpr uint32_t aesCounterSize{ 16 };
        constexpr uint32_t imageOverhead{ sizeof(application::ImageHeaderPrologue) + aesCounterSize + sizeof(application::ImageHeaderEpilogue) };
        constexpr std::array<char, 8> applicationTarget{ 'a', 'p', 'p' };

        template<std::size_t N>
        infra::BoundedConstString TextOf(const std::array<char, N>& field)
        {
            const auto end = std::find(field.begin(), field.end(), '\0');
            return infra::BoundedConstString{ field.data(), static_cast<std::size_t>(end - field.begin()) };
        }

        std::optional<uint8_t> ParseNumber(infra::BoundedConstString text)
        {
            if (text.empty())
                return std::nullopt;

            uint32_t value{ 0 };
            for (char digit : text)
            {
                if (digit < '0' || digit > '9')
                    return std::nullopt;

                value = value * 10 + static_cast<uint32_t>(digit - '0');
                if (value > std::numeric_limits<uint8_t>::max())
                    return std::nullopt;
            }

            return static_cast<uint8_t>(value);
        }

        bool FitsTheSlot(const application::ImageHeaderPrologue& image, const PackLimits& limits)
        {
            return image.lengthOfHeaderAndImage >= imageOverhead && image.lengthOfHeaderAndImage - imageOverhead <= limits.applicationCapacity;
        }

        bool NamesTheApplication(const PackHeader& header, const PackLimits& limits)
        {
            const bool oneApplicationImage = header.epilogue.numberOfImages == 1 && header.image.targetName == applicationTarget;
            const bool imageEndsThePack = header.epilogue.headerLength + header.image.lengthOfHeaderAndImage == limits.packSize;
            return oneApplicationImage && imageEndsThePack && FitsTheSlot(header.image, limits);
        }
    }

    uint32_t EpilogueOffset(const application::UpgradePackHeaderPrologue& prologue)
    {
        return sizeof(prologue) + prologue.signatureLength;
    }

    std::optional<FirmwareVersion> ParseVersion(const std::array<char, 64>& text)
    {
        auto remaining = TextOf(text);
        std::array<uint8_t, 3> numbers{};

        for (std::size_t index = 0; index != numbers.size(); ++index)
        {
            const auto separator = remaining.find('.');
            const bool last = index + 1 == numbers.size();
            if (last != (separator == infra::BoundedConstString::npos))
                return std::nullopt;

            const auto number = ParseNumber(remaining.substr(0, separator));
            if (!number)
                return std::nullopt;

            numbers[index] = *number;
            remaining = last ? infra::BoundedConstString{} : remaining.substr(separator + 1);
        }

        return FirmwareVersion{ numbers[0], numbers[1], numbers[2] };
    }

    std::optional<FirmwareVersion> CheckPackHeader(const PackHeader& header, const PackLimits& limits)
    {
        const auto& epilogue = header.epilogue;
        if (header.prologue.magic != application::upgradePackMagic || epilogue.headerVersion != headerVersion)
            return std::nullopt;

        if (epilogue.headerLength != EpilogueOffset(header.prologue) + sizeof(epilogue) || TextOf(epilogue.productName) != limits.productName)
            return std::nullopt;

        if (!NamesTheApplication(header, limits))
            return std::nullopt;

        return ParseVersion(epilogue.productVersion);
    }
}
