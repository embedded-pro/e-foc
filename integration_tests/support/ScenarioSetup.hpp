#pragma once

#include "core/foc/model/drive/memory/NvmImageBuilder.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "integration_tests/support/response/PlantTrace.hpp"
#include "motor_parameters/PlantPresets.hpp"
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace integration
{
    struct Setpoint
    {
        uint8_t messageType{ 0 };
        float value{ 0.0f };
        uint16_t nodeId{ 1 };
        std::size_t commandsBefore{ 0 };
    };

    struct StreamedCurrent
    {
        uint8_t sequence{ 0 };
        float idAmpere{ 0.0f };
        float iqAmpere{ 0.0f };
        float positionRadian{ 0.0f };
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

    struct FirmwareDownload
    {
        std::vector<uint8_t> pack;
        std::vector<uint8_t> stale;
        uint32_t crc{ 0 };
        std::optional<std::chrono::steady_clock::time_point> activatedAt;
        std::optional<uint8_t> beginAnswer;
        std::optional<uint16_t> pageSize;
        std::optional<uint8_t> verifyAnswer;
        std::optional<uint8_t> activateAnswer;
        uint16_t blocksSent{ 0 };
    };

    struct ScenarioSetup
    {
        sil::SilPlantConfig plant{ sil::NominalPlant() };
        std::string plantName{ "nominal" };
        sil::NvmImageSpec nvm{ .includeCalibration = false, .includeConfig = false };
        bool booted{ false };
        std::optional<Setpoint> lastSetpoint;
        std::optional<Setpoint> quickStop;
        std::optional<std::chrono::steady_clock::time_point> quickStopAcknowledgedAt;
        response::PlantTrace trace;
        IdentifiedParameters identified;
        IdentifiedParameters online;
        IdentifiedParameters inUse;
        std::vector<StreamedCurrent> streamedCurrents;
        FirmwareDownload download;
        // A board keeps its stored settings from one scenario to the next, unlike an emulated target
        std::vector<std::pair<can::FocParameter, can::FocParameterValue>> storedSettingsToRestore;
    };
}
