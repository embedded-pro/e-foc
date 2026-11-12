#include "targets/platform_implementations/qemu/implementation/SemihostingPlantConfig.hpp"
#include "hal/cortex_m/Semihosting.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include <array>
#include <cstring>

namespace
{
    uint32_t ReadUint32(const uint8_t* source)
    {
        uint32_t value = 0;
        std::memcpy(&value, source, sizeof(value));
        return value;
    }
}

namespace application
{
    std::optional<sil::SilPlantConfig> LoadSilPlantConfig(const char* filePath)
    {
        const auto handle = hal::cortex::SemihostingOpenFile(filePath, hal::cortex::SemihostingOpenMode::readBinary);
        if (handle < 0)
            return std::nullopt;

        std::array<uint8_t, sil::plantConfigRecordSize> record{};
        const uint32_t notRead = hal::cortex::SemihostingReadFile(handle, infra::MakeByteRange(record));
        hal::cortex::SemihostingCloseFile(handle);

        if (notRead != 0)
            return std::nullopt;

        if (ReadUint32(record.data() + sil::plantConfigMagicOffset) != sil::plantConfigMagic)
            return std::nullopt;

        if (record[sil::plantConfigVersionOffset] != sil::plantConfigLayoutVersion)
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
