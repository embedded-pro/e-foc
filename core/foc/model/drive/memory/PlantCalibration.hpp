#pragma once

#include "core/foc/model/drive/SilPlantConfig.hpp"
#include "core/foc/model/drive/memory/NvmImageBuilder.hpp"

namespace sil
{
    NvmImageSpec CalibratedFor(const SilPlantConfig& plant);
}
