#pragma once

#include "targets/platform_implementations/qemu/implementation/SilPlantConfig.hpp"
#include <optional>

namespace application
{
    // Reads the plant description the software-in-the-loop harness dropped next to the emulator's
    // working directory. Returns nothing when the file is absent, truncated, or fails its magic,
    // version or CRC check, so the caller falls back to the built-in motor.
    std::optional<sil::SilPlantConfig> LoadSilPlantConfig(const char* filePath);
}
