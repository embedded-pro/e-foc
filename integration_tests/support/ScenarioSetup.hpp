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
        uint16_t nodeId{ 1 };
        std::size_t commandsBefore{ 0 };
    };

    struct IdentifiedParameters
    {
        std::optional<float> resistanceOhm;
        std::optional<float> inductanceMilliHenry;
        std::optional<float> inertiaKgM2;
        std::optional<float> frictionNmSPerRad;
        std::optional<unsigned> polePairs;
        std::optional<float> currentOffsetAmpereA;
        std::optional<float> currentOffsetAmpereB;
        std::optional<float> currentOffsetAmpereC;
        std::optional<float> coggingPeakNm;
        std::optional<float> fluxLinkageWeber;
    };

    struct ScenarioSetup
    {
        sil::SilPlantConfig plant{ sil::NominalPlant() };
        std::string plantName{ "nominal" };
        sil::NvmImageSpec nvm{ .includeCalibration = false, .includeConfig = false };
        bool booted{ false };
        std::optional<Setpoint> lastSetpoint;
        response::PlantTrace trace;
        IdentifiedParameters identified;
        IdentifiedParameters online;
    };
}
