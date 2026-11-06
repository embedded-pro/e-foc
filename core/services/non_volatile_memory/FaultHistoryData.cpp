#include "core/services/non_volatile_memory/FaultHistoryData.hpp"
#include "infra/util/Crc.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services
{
    namespace
    {
        constexpr std::size_t powerUpsOffset{ 5 };
        constexpr std::size_t powerUpsCheckOffset{ 7 };
        constexpr std::size_t recordCheckOffset{ faultRecordSize - 1 };

        uint8_t Check(infra::ConstByteRange bytes)
        {
            infra::Crc8Maxim crc;
            crc.Update(bytes);
            return crc.Result();
        }

        template<class T>
        void Put(infra::ByteRange image, std::size_t offset, T value)
        {
            for (std::size_t byte = 0; byte != sizeof(T); ++byte)
                image[offset + byte] = static_cast<uint8_t>(static_cast<std::make_unsigned_t<T>>(value) >> (8 * byte));
        }

        template<class T>
        T Get(infra::ConstByteRange image, std::size_t offset)
        {
            std::make_unsigned_t<T> value{ 0 };
            for (std::size_t byte = 0; byte != sizeof(T); ++byte)
                value |= static_cast<std::make_unsigned_t<T>>(static_cast<std::make_unsigned_t<T>>(image[offset + byte]) << (8 * byte));
            return static_cast<T>(value);
        }

        void PutRecord(infra::ByteRange slot, const std::optional<FaultRecord>& record)
        {
            if (!record.has_value())
            {
                std::fill(slot.begin(), slot.end(), uint8_t{ 0xFF });
                return;
            }

            slot[0] = record->code;
            slot[1] = record->mode;
            Put<uint16_t>(slot, 2, record->powerUp);
            Put<uint32_t>(slot, 4, record->uptimeMs);
            Put<int16_t>(slot, 8, record->busVoltageCentivolts);
            Put<int16_t>(slot, 10, record->peakCurrentCentiamperes);
            slot[recordCheckOffset] = Check(infra::Head(slot, recordCheckOffset));
        }

        std::optional<FaultRecord> GetRecord(infra::ConstByteRange slot)
        {
            if (slot[recordCheckOffset] != Check(infra::Head(slot, recordCheckOffset)) || slot[0] == 0)
                return std::nullopt;

            return FaultRecord{ slot[0], slot[1], Get<uint16_t>(slot, 2), Get<uint32_t>(slot, 4), Get<int16_t>(slot, 8), Get<int16_t>(slot, 10) };
        }

        infra::ByteRange Slot(infra::ByteRange image, std::size_t index)
        {
            return infra::Head(infra::DiscardHead(image, faultHistoryHeaderSize + index * faultRecordSize), faultRecordSize);
        }

        infra::ConstByteRange Slot(infra::ConstByteRange image, std::size_t index)
        {
            return infra::Head(infra::DiscardHead(image, faultHistoryHeaderSize + index * faultRecordSize), faultRecordSize);
        }
    }

    void SerializeFaultHistory(const FaultHistoryData& history, infra::ByteRange image)
    {
        really_assert(image.size() == faultHistoryImageSize);

        Put<uint32_t>(image, 0, FaultHistoryMagic);
        image[4] = FaultHistoryLayoutVersion;
        Put<uint16_t>(image, powerUpsOffset, history.powerUps);
        image[powerUpsCheckOffset] = Check(infra::Head(infra::DiscardHead(image, powerUpsOffset), sizeof(uint16_t)));

        for (std::size_t index = 0; index != FaultHistoryData::capacity; ++index)
            PutRecord(Slot(image, index), history.records[index]);
    }

    std::optional<FaultHistoryData> DeserializeFaultHistory(infra::ConstByteRange image)
    {
        really_assert(image.size() == faultHistoryImageSize);

        if (Get<uint32_t>(image, 0) != FaultHistoryMagic || image[4] != FaultHistoryLayoutVersion)
            return std::nullopt;

        FaultHistoryData history;
        const auto powerUps = infra::Head(infra::DiscardHead(image, powerUpsOffset), sizeof(uint16_t));
        if (image[powerUpsCheckOffset] == Check(powerUps))
            history.powerUps = Get<uint16_t>(image, powerUpsOffset);

        for (std::size_t index = 0; index != FaultHistoryData::capacity; ++index)
            history.records[index] = GetRecord(Slot(image, index));

        return history;
    }
}
