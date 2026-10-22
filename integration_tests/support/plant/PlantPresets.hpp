#pragma once

#include "motor_parameters/AnaheimBly172s24v4000.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include "targets/platform_implementations/motor_boards/E-FOC-HARDWARE/BoardCharacteristics.hpp"
#include "targets/platform_implementations/qemu/implementation/SilPlantConfig.hpp"
#include <string>

namespace sil
{
    inline void DescribeMotor(SilPlantConfig& config, const foc::ThreePhaseMotorModel::Parameters& motor, foc::Volts supply)
    {
        config.statorResistanceOhm = motor.R.Value();
        config.dAxisInductanceHenry = motor.Ld.Value();
        config.qAxisInductanceHenry = motor.Lq.Value();
        config.fluxLinkageWeber = motor.psi_f.Value();
        config.rotorInertiaKgM2 = motor.J.Value();
        config.viscousDampingNmSPerRad = motor.B.Value();
        config.maxSupportedCurrentAmpere = motor.maxSupportedCurrent.Value();
        config.powerSupplyVoltageVolts = supply.Value();
        config.polePairs = motor.p;
    }

    inline void DescribeBenchCurrentSense(SilPlantConfig& config)
    {
        config.adcClockMegahertz = 16.0f;
        config.adcConversionClocks = 12u;
        config.adcAveragedConversions = 2u;
        config.currentAmplifierTimeConstantNanoseconds = 200.0f;
        config.currentFullScaleAmpere = 8.25f;
        config.adcResolutionBits = 12u;
        config.maxReadableDutyPercent = 70u;
        config.maxDutyPercent = application::BoardCharacteristics::maxDutyPercent;
        config.noiseSigmaAmpere = 0.05f;
        config.noiseBiasAmpereA = 0.027f;
        config.noiseBiasAmpereB = -0.018f;
        config.noiseBiasAmpereC = 0.054f;
    }

    inline void DescribeBenchPowerStage(SilPlantConfig& config)
    {
        config.deadTimeNanoseconds = 100.0f;
        config.switchNodeCapacitancePicofarad = 600.0f;
        config.switchOnResistanceOhm = 0.0125f;
        config.diodeForwardVolts = 0.8f;
    }

    inline void DescribeBenchFrictionAndCogging(SilPlantConfig& config)
    {
        config.coulombFrictionNm = 0.0037f;
        config.coggingTorqueNm = 0.004f;
        config.coggingCyclesPerRevolution = 24u;
    }

    inline void DescribeBoardPwmUpdate(SilPlantConfig& config)
    {
        config.dutyDelayPeriods = 1u;
    }

    inline void DescribeBenchDeadTimeCompensation(SilPlantConfig& config)
    {
        config.deadTimeCompensationNanoseconds = static_cast<float>(application::BoardCharacteristics::effectiveDeadTimeNanoseconds);
        config.deadTimeCompensationKneeAmpere = application::BoardCharacteristics::deadTimeKneeAmps;
    }

    inline SilPlantConfig NominalPlant()
    {
        SilPlantConfig config{
            .statorResistanceOhm = 0.0f,
            .dAxisInductanceHenry = 0.0f,
            .qAxisInductanceHenry = 0.0f,
            .fluxLinkageWeber = 0.0f,
            .rotorInertiaKgM2 = 0.0f,
            .viscousDampingNmSPerRad = 0.0f,
            .maxSupportedCurrentAmpere = 0.0f,
            .loadTorqueNm = 0.0f,

            .powerSupplyVoltageVolts = 0.0f,
            .baseFrequencyHz = 20000u,

            .noiseSigmaAmpere = 0.0f,
            .noiseBiasAmpereA = 0.0f,
            .noiseBiasAmpereB = 0.0f,
            .noiseBiasAmpereC = 0.0f,

            .ambientCelsius = 25.0f,
            .thermalResistance = 2.0f,
            .thermalCapacitance = 25.0f,
            .copperTempCoeff = 0.00393f,
            .ironInductanceCoeff = 0.0f,

            .encoderSigmaRadians = 0.0f,
            .encoderBiasRadians = 0.0f,

            .overCurrentTripAmpere = 0.0f,
            .overVoltageTripVolts = 0.0f,
            .underVoltageTripVolts = 0.0f,
            .overTemperatureTripCelsius = 0.0f,

            .supplyVoltageScale = 1.0f,
            .randomSeed = 20260921u,

            .torqueStepNm = 0.0f,
            .torqueStepDelayMs = 0u,
            .responseSampleRateHz = 0u,
            .responseMaxSamples = 0u,

            .coulombFrictionNm = 0.0f,
            .coggingTorqueNm = 0.0f,
            .coggingCyclesPerRevolution = 0u,
            .supplySourceResistanceOhm = 0.0f,
            .supplyCurrentLimitAmpere = 0.0f,
            .encoderCountsPerRevolution = 0u,

            .deadTimeNanoseconds = 0.0f,
            .switchNodeCapacitancePicofarad = 0.0f,
            .switchOnResistanceOhm = 0.0f,
            .diodeForwardVolts = 0.0f,
            .minimumPulseNanoseconds = 0.0f,

            .adcClockMegahertz = 0.0f,
            .adcSampleHoldClocks = 0u,
            .adcConversionClocks = 0u,
            .adcAveragedConversions = 0u,
            .adcTriggerDelayNanoseconds = 0.0f,
            .currentAmplifierTimeConstantNanoseconds = 0.0f,
            .currentFullScaleAmpere = 0.0f,
            .adcResolutionBits = 0u,
            .maxReadableDutyPercent = 0u,
            .maxDutyPercent = 0u,

            .deadTimeCompensationNanoseconds = 0.0f,
            .deadTimeCompensationKneeAmpere = 0.0f,

            .polePairs = 0u,
            .faultFlags = 0u,
            .encoderFreezeDelayCentiseconds = 0u,
            .dutyDelayPeriods = 0u,
        };

        DescribeMotor(config, foc::M_2310P_LN_04K::parameters, foc::M_2310P_LN_04K::ratedSupply);
        return config;
    }

    inline bool TryNamedPlant(const std::string& name, SilPlantConfig& config)
    {
        config = NominalPlant();

        if (name == "nominal" || name == "teknic")
            return true;

        if (name == "anaheim")
        {
            DescribeMotor(config, foc::BLY172S_24V_4000::parameters, foc::BLY172S_24V_4000::ratedSupply);
            return true;
        }

        if (name == "bench")
        {
            config.statorResistanceOhm = 1.10f;
            config.dAxisInductanceHenry = 0.00107f;
            config.qAxisInductanceHenry = 0.00107f;
            config.fluxLinkageWeber = 0.007f;
            config.rotorInertiaKgM2 = 0.0000124f;
            config.viscousDampingNmSPerRad = 0.000005f;
            config.coulombFrictionNm = 0.0005f;
            config.maxSupportedCurrentAmpere = 3.0f;
            config.powerSupplyVoltageVolts = 24.0f;
            config.polePairs = 4u;
            config.encoderCountsPerRevolution = 4000u;
            config.noiseSigmaAmpere = 0.01f;
            return true;
        }

        if (name == "noisy")
        {
            config.noiseSigmaAmpere = 0.05f;
            config.noiseBiasAmpereA = 0.02f;
            config.encoderSigmaRadians = 0.0002f;
            return true;
        }

        if (name == "hot")
        {
            config.ambientCelsius = 90.0f;
            config.thermalCapacitance = 0.5f;
            config.thermalResistance = 20.0f;
            return true;
        }

        if (name == "loaded")
        {
            config.loadTorqueNm = 0.02f;
            return true;
        }

        if (name == "disconnected")
        {
            config.faultFlags = PlantFaultFlag::openPhaseA | PlantFaultFlag::openPhaseB | PlantFaultFlag::openPhaseC;
            return true;
        }

        if (name == "open phase")
        {
            config.faultFlags = PlantFaultFlag::openPhaseA;
            return true;
        }

        if (name == "stuck encoder")
        {
            config.faultFlags = PlantFaultFlag::encoderStuck;
            return true;
        }

        return false;
    }
}
