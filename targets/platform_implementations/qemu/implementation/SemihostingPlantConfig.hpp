#pragma once

#include "core/foc/model/drive/SilPlantConfig.hpp"
#include <optional>

namespace application
{
    std::optional<sil::SilPlantConfig> LoadSilPlantConfig(const char* filePath);
}
