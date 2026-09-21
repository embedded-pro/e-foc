#pragma once

#include "targets/platform_implementations/qemu/implementation/SilPlantConfig.hpp"
#include <cstdint>
#include <vector>

namespace sil
{
    std::vector<uint8_t> EncodePlantConfig(const SilPlantConfig& config);
}
