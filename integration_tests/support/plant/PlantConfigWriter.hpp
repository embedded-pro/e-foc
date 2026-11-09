#pragma once

#include "core/foc/model/drive/SilPlantConfig.hpp"
#include <cstdint>
#include <vector>

namespace sil
{
    std::vector<uint8_t> EncodePlantConfig(const SilPlantConfig& config);
}
