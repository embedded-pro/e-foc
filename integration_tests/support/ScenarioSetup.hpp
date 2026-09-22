#pragma once

#include "integration_tests/support/nvm/NvmImageBuilder.hpp"
#include "integration_tests/support/plant/PlantPresets.hpp"
#include "integration_tests/support/response/PlantTrace.hpp"
#include <cstddef>
#include <optional>
#include <string>

namespace integration
{
    struct Setpoint
    {
        uint8_t messageType{ 0 };
        float value{ 0.0f };
    };

    struct IdentifiedParameters
    {
        std::optional<float> resistanceOhm;
        std::optional<float> inductanceMilliHenry;
        std::optional<float> inertiaKgM2;
        std::optional<float> frictionNmSPerRad;
        std::optional<unsigned> polePairs;
    };

    struct ScenarioSetup
    {
        sil::SilPlantConfig plant{ sil::NominalPlant() };
        std::string plantName{ "nominal" };
        sil::NvmImageSpec nvm{ .includeCalibration = false, .includeConfig = false };
        bool booted{ false };
        std::optional<Setpoint> lastSetpoint;
        response::PlantTrace trace;
        std::size_t consumedLines{ 0 };
        IdentifiedParameters identified;
        IdentifiedParameters online;
    };
}
