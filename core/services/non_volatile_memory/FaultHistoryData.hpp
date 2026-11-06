#pragma once

#include "infra/util/ByteRange.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    struct FaultRecord
    {
        uint8_t code{ 0 };
        uint8_t mode{ 0 };
        uint16_t powerUp{ 0 };
        uint32_t uptimeMs{ 0 };
        int16_t busVoltageCentivolts{ 0 };
        int16_t peakCurrentCentiamperes{ 0 };

        bool operator==(const FaultRecord&) const = default;
    };

    struct FaultHistoryData
    {
        static constexpr std::size_t capacity{ 8 };

        uint16_t powerUps{ 0 };
        std::array<std::optional<FaultRecord>, capacity> records{};
    };

    // On-media layout in service-nvm.md, Fault History Region: each part carries its own CRC-8.
    static constexpr uint32_t FaultHistoryMagic = 0xFA17C0DE;
    static constexpr uint8_t FaultHistoryLayoutVersion = 1;
    static constexpr std::size_t faultHistoryHeaderSize{ 8 };
    static constexpr std::size_t faultRecordSize{ 13 };
    static constexpr std::size_t faultHistoryImageSize{ faultHistoryHeaderSize + FaultHistoryData::capacity * faultRecordSize };

    void SerializeFaultHistory(const FaultHistoryData& history, infra::ByteRange image);
    std::optional<FaultHistoryData> DeserializeFaultHistory(infra::ConstByteRange image);
}
