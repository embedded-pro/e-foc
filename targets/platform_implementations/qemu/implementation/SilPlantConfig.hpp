#pragma once

#include <cstddef>
#include <cstdint>

namespace sil
{
    namespace PlantFaultFlag
    {
        static constexpr uint8_t openPhaseA = 1u << 0;
        static constexpr uint8_t openPhaseB = 1u << 1;
        static constexpr uint8_t openPhaseC = 1u << 2;
        static constexpr uint8_t encoderStuck = 1u << 3;
    }

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

        float overCurrentTripAmpere;
        float overVoltageTripVolts;
        float underVoltageTripVolts;
        float overTemperatureTripCelsius;

        float supplyVoltageScale;
        uint32_t randomSeed;

        float torqueStepNm;
        uint32_t torqueStepDelayMs;
        uint32_t responseSampleRateHz;
        uint32_t responseMaxSamples;

        uint8_t polePairs;
        uint8_t faultFlags;
        // Carried in centiseconds so it fits the byte this record already reserved; zero means the
        // encoder is never frozen while running, which is what every scenario that does not ask
        // for it writes. The boot-time encoderStuck flag is the separate case of an encoder that
        // was already dead before the drive aligned.
        uint8_t encoderFreezeDelayCentiseconds;
        uint8_t reserved1;
    };

    static_assert(sizeof(SilPlantConfig) == 128, "SilPlantConfig layout must be free of implicit padding");

    static constexpr uint32_t plantConfigMagic = 0x504C4E54;
    static constexpr uint8_t plantConfigLayoutVersion = 2;

    static constexpr std::size_t plantConfigRecordSize =
        sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(SilPlantConfig);

    static constexpr std::size_t plantConfigMagicOffset = 0;
    static constexpr std::size_t plantConfigVersionOffset = sizeof(uint32_t);
    static constexpr std::size_t plantConfigCrcOffset = plantConfigVersionOffset + sizeof(uint8_t);
    static constexpr std::size_t plantConfigDataOffset = plantConfigCrcOffset + sizeof(uint32_t);

    static constexpr const char* plantConfigFileName = "plant.bin";
}
