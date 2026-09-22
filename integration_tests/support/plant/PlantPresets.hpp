#pragma once

#include "motor_parameters/AnaheimBly172s24v4000.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
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

            .polePairs = 0u,
            .faultFlags = 0u,
            .reserved0 = 0u,
            .reserved1 = 0u,
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
