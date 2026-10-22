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

        float coulombFrictionNm;
        float coggingTorqueNm;
        uint32_t coggingCyclesPerRevolution;
        float supplySourceResistanceOhm;
        float supplyCurrentLimitAmpere;
        uint32_t encoderCountsPerRevolution;

        float deadTimeNanoseconds;
        float switchNodeCapacitancePicofarad;
        float switchOnResistanceOhm;
        float diodeForwardVolts;
        float minimumPulseNanoseconds;

        float adcClockMegahertz;
        uint32_t adcSampleHoldClocks;
        uint32_t adcConversionClocks;
        uint32_t adcAveragedConversions;
        float adcTriggerDelayNanoseconds;
        float currentAmplifierTimeConstantNanoseconds;
        float currentFullScaleAmpere;
        uint32_t adcResolutionBits;
        uint32_t maxReadableDutyPercent;
        uint32_t maxDutyPercent;

        float deadTimeCompensationNanoseconds;
        float deadTimeCompensationKneeAmpere;

        uint8_t polePairs;
        uint8_t faultFlags;
        // Carried in centiseconds so it fits the byte this record already reserved; zero means the
        // encoder is never frozen while running, which is what every scenario that does not ask
        // for it writes. The boot-time encoderStuck flag is the separate case of an encoder that
        // was already dead before the drive aligned.
        uint8_t encoderFreezeDelayCentiseconds;
        uint8_t dutyDelayPeriods;
    };

    static_assert(sizeof(SilPlantConfig) == 220, "SilPlantConfig layout must be free of implicit padding");

    static constexpr uint32_t plantConfigMagic = 0x504C4E54;
    static constexpr uint8_t plantConfigLayoutVersion = 8;

    static constexpr std::size_t plantConfigRecordSize =
        sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(SilPlantConfig);

    static constexpr std::size_t plantConfigMagicOffset = 0;
    static constexpr std::size_t plantConfigVersionOffset = sizeof(uint32_t);
    static constexpr std::size_t plantConfigCrcOffset = plantConfigVersionOffset + sizeof(uint8_t);
    static constexpr std::size_t plantConfigDataOffset = plantConfigCrcOffset + sizeof(uint32_t);

    static constexpr const char* plantConfigFileName = "plant.bin";
}
