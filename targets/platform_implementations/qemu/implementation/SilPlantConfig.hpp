#pragma once

#include <cstddef>
#include <cstdint>

namespace sil
{
    // Wire contract between the software-in-the-loop harness, which writes the blob, and the
    // QEMU firmware, which reads it at boot. Kept free of unit types and of any e-foc header so
    // both sides agree on a byte layout without sharing a build target.
    namespace PlantFaultFlag
    {
        static constexpr uint8_t openPhaseA = 1u << 0;
        static constexpr uint8_t openPhaseB = 1u << 1;
        static constexpr uint8_t openPhaseC = 1u << 2;
        static constexpr uint8_t encoderStuck = 1u << 3;
    }

    // All 4-byte fields precede the byte-sized fields to avoid implicit compiler padding.
    struct SilPlantConfig
    {
        float statorResistanceOhm;
        float dAxisInductanceHenry;
        float qAxisInductanceHenry;
        float fluxLinkageWeber;
        float rotorInertiaKgM2;
        float viscousDampingNmSPerRad;
        float maxSupportedCurrentAmpere;
        float loadTorqueNm;

        float powerSupplyVoltageVolts;
        uint32_t baseFrequencyHz;

        float noiseSigmaAmpere;
        float noiseBiasAmpereA;
        float noiseBiasAmpereB;
        float noiseBiasAmpereC;

        float ambientCelsius;
        float thermalResistance;
        float thermalCapacitance;
        float copperTempCoeff;
        float ironInductanceCoeff;

        float encoderSigmaRadians;
        float encoderBiasRadians;

        // A non-positive trip disables that protection, which is how a scenario opts out of one.
        float overCurrentTripAmpere;
        float overVoltageTripVolts;
        float underVoltageTripVolts;
        float overTemperatureTripCelsius;

        float supplyVoltageScale;
        uint32_t randomSeed;

        uint8_t polePairs;
        uint8_t faultFlags;
        uint8_t reserved0;
        uint8_t reserved1;
    };

    static_assert(sizeof(SilPlantConfig) == 112, "SilPlantConfig layout must be free of implicit padding");

    static constexpr uint32_t plantConfigMagic = 0x504C4E54;
    static constexpr uint8_t plantConfigLayoutVersion = 1;

    // Record layout: [magic:4][version:1][crc32:4][data:N], matching the NVM record convention.
    // The CRC covers the payload only.
    static constexpr std::size_t plantConfigRecordSize =
        sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(SilPlantConfig);

    static constexpr std::size_t plantConfigMagicOffset = 0;
    static constexpr std::size_t plantConfigVersionOffset = sizeof(uint32_t);
    static constexpr std::size_t plantConfigCrcOffset = plantConfigVersionOffset + sizeof(uint8_t);
    static constexpr std::size_t plantConfigDataOffset = plantConfigCrcOffset + sizeof(uint32_t);

    static constexpr const char* plantConfigFileName = "plant.bin";
}
