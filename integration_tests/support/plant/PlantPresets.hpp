#pragma once

#include "motor_parameters/Jk42bls01X038ed.hpp"
#include "targets/platform_implementations/qemu/implementation/SilPlantConfig.hpp"
#include <string>

namespace sil
{
    // The nominal plant every scenario starts from: the catalogue motor on a healthy 48 V bus,
    // noiseless, with protection thresholds generous enough that a well-behaved run never trips.
    inline SilPlantConfig NominalPlant()
    {
        const auto& motor = foc::JK42BLS01_X038ED::parameters;

        return SilPlantConfig{
            .statorResistanceOhm = motor.R.Value(),
            .dAxisInductanceHenry = motor.Ld.Value(),
            .qAxisInductanceHenry = motor.Lq.Value(),
            .fluxLinkageWeber = motor.psi_f.Value(),
            .rotorInertiaKgM2 = motor.J.Value(),
            .viscousDampingNmSPerRad = motor.B.Value(),
            .maxSupportedCurrentAmpere = motor.maxSupportedCurrent.Value(),
            .loadTorqueNm = 0.0f,

            .powerSupplyVoltageVolts = 48.0f,
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

            .polePairs = motor.p,
            .faultFlags = 0u,
            .reserved0 = 0u,
            .reserved1 = 0u,
        };
    }

    // Named starting points a scenario selects by name and then overrides field by field.
    inline bool TryNamedPlant(const std::string& name, SilPlantConfig& config)
    {
        config = NominalPlant();

        if (name == "nominal")
            return true;

        if (name == "noisy")
        {
            config.noiseSigmaAmpere = 0.05f;
            config.noiseBiasAmpereA = 0.02f;
            config.encoderSigmaRadians = 0.001f;
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
