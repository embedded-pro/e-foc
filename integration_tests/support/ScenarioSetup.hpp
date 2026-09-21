#pragma once

#include "integration_tests/support/nvm/NvmImageBuilder.hpp"
#include "integration_tests/support/plant/PlantPresets.hpp"

namespace integration
{
    // Accumulated by the Given steps and applied in one target restart, so a scenario that
    // describes a plant, a calibration and a set of algorithms still boots the emulator once.
    struct ScenarioSetup
    {
        sil::SilPlantConfig plant{ sil::NominalPlant() };
        sil::NvmImageSpec nvm{ .includeCalibration = false, .includeConfig = false };
        bool booted{ false };
    };
}
