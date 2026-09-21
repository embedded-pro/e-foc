#pragma once

#include "targets/platform_implementations/qemu/implementation/SilPlantConfig.hpp"
#include <optional>

namespace application
{
    std::optional<sil::SilPlantConfig> LoadSilPlantConfig(const char* filePath);
}
