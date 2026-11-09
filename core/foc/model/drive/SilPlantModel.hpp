#pragma once

#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/model/drive/SilPlantConfig.hpp"
#include "core/platform_abstraction/LowSideShuntWindow.hpp"
#include <cstdint>
#include <optional>

namespace sil
{
    inline foc::ThreePhaseMotorModel::Parameters MotorParameters(const SilPlantConfig& config)
    {
        return foc::ThreePhaseMotorModel::Parameters{
            .R = foc::Ohm{ config.statorResistanceOhm },
            .Ld = foc::Henry{ config.dAxisInductanceHenry },
            .Lq = foc::Henry{ config.qAxisInductanceHenry },
            .psi_f = foc::Weber{ config.fluxLinkageWeber },
            .p = config.polePairs,
            .J = foc::KilogramMeterSquared{ config.rotorInertiaKgM2 },
            .B = foc::NewtonMeterSecondPerRadian{ config.viscousDampingNmSPerRad },
            .maxSupportedCurrent = foc::Ampere{ config.maxSupportedCurrentAmpere },
        };
    }

    inline foc::ThreePhaseMotorModel::FaultInjectionConfig BootFaultInjection(const SilPlantConfig& config)
    {
        return foc::ThreePhaseMotorModel::FaultInjectionConfig{
            .openPhaseA = (config.faultFlags & PlantFaultFlag::openPhaseA) != 0,
            .openPhaseB = (config.faultFlags & PlantFaultFlag::openPhaseB) != 0,
            .openPhaseC = (config.faultFlags & PlantFaultFlag::openPhaseC) != 0,
            .encoderStuck = (config.faultFlags & PlantFaultFlag::encoderStuck) != 0,
            .supplyVoltageScale = config.supplyVoltageScale,
        };
    }

    inline foc::CurrentSenseConfig CurrentSense(const SilPlantConfig& config, uint32_t requestedSampleHoldClocks)
    {
        return foc::CurrentSenseConfig{
            .adcClockHertz = config.adcClockMegahertz * 1e6f,
            .sampleHoldClocks = config.adcSampleHoldClocks != 0 ? config.adcSampleHoldClocks : requestedSampleHoldClocks,
            .conversionClocks = config.adcConversionClocks,
            .averagedConversions = config.adcAveragedConversions,
            .triggerDelaySeconds = config.adcTriggerDelayNanoseconds * 1e-9f,
            .amplifierTimeConstantSeconds = config.currentAmplifierTimeConstantNanoseconds * 1e-9f,
            .fullScaleAmpere = config.currentFullScaleAmpere,
            .resolutionBits = config.adcResolutionBits,
        };
    }

    inline foc::PhaseSampleDelays SampleDelays(const foc::CurrentSenseConfig& chain)
    {
        return foc::SequentialPhaseSampleDelays({
            .converterClockHertz = chain.adcClockHertz,
            .sampleHoldClocks = chain.sampleHoldClocks,
            .conversionClocks = chain.conversionClocks,
            .averagedConversions = chain.averagedConversions,
            .triggerDelay = std::chrono::duration<float>{ chain.triggerDelaySeconds },
        });
    }

    inline void ConfigureModel(foc::ThreePhaseMotorModel& model, const SilPlantConfig& config)
    {
        foc::SeedNoise(model, config.randomSeed);
        model.CurrentSensor().SetNoise({
            .sigmaAmpere = config.noiseSigmaAmpere,
            .biasAmpereA = config.noiseBiasAmpereA,
            .biasAmpereB = config.noiseBiasAmpereB,
            .biasAmpereC = config.noiseBiasAmpereC,
        });
        model.Thermal().Configure({
            .ambientCelsius = config.ambientCelsius,
            .thermalResistance = config.thermalResistance,
            .thermalCapacitance = config.thermalCapacitance,
            .copperTempCoeff = config.copperTempCoeff,
            .ironInductanceCoeff = config.ironInductanceCoeff,
        });
        model.Encoder().SetNoise({
            .sigmaRadians = config.encoderSigmaRadians,
            .biasRadians = config.encoderBiasRadians,
        });
        foc::InjectFaults(model, BootFaultInjection(config));
        model.Supply().Configure({
            .sourceResistanceOhm = config.supplySourceResistanceOhm,
            .currentLimitAmpere = config.supplyCurrentLimitAmpere,
        });
        model.Electrical().SetPowerStage({
            .deadTimeSeconds = config.deadTimeNanoseconds * 1e-9f,
            .switchNodeCapacitanceFarad = config.switchNodeCapacitancePicofarad * 1e-12f,
            .onResistanceOhm = config.switchOnResistanceOhm,
            .diodeForwardVolts = config.diodeForwardVolts,
            .minimumPulseSeconds = config.minimumPulseNanoseconds * 1e-9f,
        });
        model.Mechanics().SetCoulombFriction(foc::NewtonMeter{ config.coulombFrictionNm });
        model.Mechanics().SetCogging(foc::NewtonMeter{ config.coggingTorqueNm }, config.coggingCyclesPerRevolution);
        model.Encoder().SetResolution(config.encoderCountsPerRevolution);
        model.Encoder().SetIndex(config.encoderIndexWidthCounts != 0 ? std::make_optional(foc::Radians{ config.encoderIndexRadians }) : std::nullopt, config.encoderIndexWidthCounts);
        model.Thermal().Reset();

        if (config.loadTorqueNm != 0.0f)
            model.Mechanics().SetLoad(foc::NewtonMeter{ config.loadTorqueNm });
    }
}
