#include "integration_tests/support/plant/PlantConfigWriter.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include <cstring>

namespace
{
    void AppendUint32(std::vector<uint8_t>& out, uint32_t value)
    {
        uint8_t bytes[sizeof(value)]{};
        std::memcpy(bytes, &value, sizeof(value));
        out.insert(out.end(), std::begin(bytes), std::end(bytes));
    }
}

namespace sil
{
    std::vector<uint8_t> EncodePlantConfig(const SilPlantConfig& config)
    {
        infra::Crc32 crc;
        crc.Update(infra::MakeConstByteRange(config));

        std::vector<uint8_t> record;
        record.reserve(plantConfigRecordSize);

        AppendUint32(record, plantConfigMagic);
        record.push_back(plantConfigLayoutVersion);
        AppendUint32(record, crc.Result());

        const auto* payload = reinterpret_cast<const uint8_t*>(&config);
        record.insert(record.end(), payload, payload + sizeof(config));

        return record;
    }
}
