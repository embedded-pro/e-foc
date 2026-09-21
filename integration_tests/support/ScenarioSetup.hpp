#pragma once

#include "integration_tests/support/nvm/NvmImageBuilder.hpp"
#include "integration_tests/support/plant/PlantPresets.hpp"

namespace integration
{
    struct ScenarioSetup
    {
        sil::SilPlantConfig plant{ sil::NominalPlant() };
        sil::NvmImageSpec nvm{ .includeCalibration = false, .includeConfig = false };
        bool booted{ false };
    };
}
