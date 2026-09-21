#pragma once

#include "targets/platform_implementations/qemu/implementation/SilPlantConfig.hpp"
#include <cstdint>
#include <vector>

namespace sil
{
    // Serialises a plant description into the record the QEMU firmware reads at boot:
    // [magic:4][version:1][crc32:4][data:N], CRC over the payload only.
    std::vector<uint8_t> EncodePlantConfig(const SilPlantConfig& config);
}
