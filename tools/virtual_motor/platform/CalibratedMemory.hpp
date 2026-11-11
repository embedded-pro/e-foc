#pragma once

#include "core/foc/model/drive/SilPlantConfig.hpp"
#include <string>

namespace virtual_motor
{
    bool WriteCalibratedMemory(const std::string& memoryFile, const sil::SilPlantConfig& plant);
}
