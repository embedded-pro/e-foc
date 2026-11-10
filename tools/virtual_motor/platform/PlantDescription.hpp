#pragma once

#include "core/foc/model/drive/SilPlantConfig.hpp"
#include <optional>
#include <string>

namespace virtual_motor
{
    std::optional<sil::SilPlantConfig> PlantDescription(const std::string& presetOrFile);
}
