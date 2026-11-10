#include "tools/virtual_motor/platform/PlantDescription.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include <array>
#include <cstring>
#include <fstream>

namespace virtual_motor
{
    namespace
    {
        uint32_t ReadUint32(const uint8_t* source)
        {
            uint32_t value{ 0 };
            std::memcpy(&value, source, sizeof(value));
            return value;
        }

        std::optional<sil::SilPlantConfig> ReadPlantFile(const std::string& path)
        {
            std::ifstream file{ path, std::ios::binary };
            std::array<uint8_t, sil::plantConfigRecordSize> record{};
            if (!file.read(reinterpret_cast<char*>(record.data()), record.size()))
                return std::nullopt;

            if (ReadUint32(record.data() + sil::plantConfigMagicOffset) != sil::plantConfigMagic || record[sil::plantConfigVersionOffset] != sil::plantConfigLayoutVersion)
                return std::nullopt;

            sil::SilPlantConfig config{};
            std::memcpy(&config, record.data() + sil::plantConfigDataOffset, sizeof(config));

            infra::Crc32 crc;
            crc.Update(infra::MakeConstByteRange(config));
            if (crc.Result() != ReadUint32(record.data() + sil::plantConfigCrcOffset))
                return std::nullopt;

            return config;
        }
    }

    std::optional<sil::SilPlantConfig> PlantDescription(const std::string& presetOrFile)
    {
        sil::SilPlantConfig config{};
        if (sil::TryNamedPlant(presetOrFile, config))
            return config;

        return ReadPlantFile(presetOrFile);
    }
}
