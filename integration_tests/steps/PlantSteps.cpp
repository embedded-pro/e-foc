#include "core/can/FocMotorMessages.hpp"
#include "cucumber_cpp/Steps.hpp"
#include "integration_tests/support/Fixture.hpp"
#include "integration_tests/support/ScenarioSetup.hpp"
#include "integration_tests/support/plant/PlantConfigWriter.hpp"
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>

using namespace integration;

namespace
{
    bool ApplyPlantOverride(sil::SilPlantConfig& plant, const std::string& key, const std::string& rawValue)
    {
        const float value = std::strtof(rawValue.c_str(), nullptr);

        if (key == "stator_resistance_ohm")
            plant.statorResistanceOhm = value;
        else if (key == "d_axis_inductance_henry")
            plant.dAxisInductanceHenry = value;
        else if (key == "q_axis_inductance_henry")
            plant.qAxisInductanceHenry = value;
        else if (key == "flux_linkage_weber")
            plant.fluxLinkageWeber = value;
        else if (key == "rotor_inertia_kg_m2")
            plant.rotorInertiaKgM2 = value;
        else if (key == "viscous_damping_nm_s_per_rad")
            plant.viscousDampingNmSPerRad = value;
        else if (key == "max_supported_current_ampere")
            plant.maxSupportedCurrentAmpere = value;
        else if (key == "load_torque_nm")
            plant.loadTorqueNm = value;
        else if (key == "supply_voltage_volts")
            plant.powerSupplyVoltageVolts = value;
        else if (key == "base_frequency_hz")
            plant.baseFrequencyHz = static_cast<uint32_t>(std::strtoul(rawValue.c_str(), nullptr, 10));
        else if (key == "pole_pairs")
            plant.polePairs = static_cast<uint8_t>(std::strtoul(rawValue.c_str(), nullptr, 10));
        else if (key == "adc_noise_sigma_ampere")
            plant.noiseSigmaAmpere = value;
        else if (key == "adc_bias_ampere_a")
            plant.noiseBiasAmpereA = value;
        else if (key == "adc_bias_ampere_b")
            plant.noiseBiasAmpereB = value;
        else if (key == "adc_bias_ampere_c")
            plant.noiseBiasAmpereC = value;
        else if (key == "ambient_celsius")
            plant.ambientCelsius = value;
        else if (key == "thermal_resistance")
            plant.thermalResistance = value;
        else if (key == "thermal_capacitance")
            plant.thermalCapacitance = value;
        else if (key == "copper_temp_coeff")
            plant.copperTempCoeff = value;
        else if (key == "iron_inductance_coeff")
            plant.ironInductanceCoeff = value;
        else if (key == "encoder_noise_sigma_radians")
            plant.encoderSigmaRadians = value;
        else if (key == "torque_step_nm")
            plant.torqueStepNm = value;
        else if (key == "torque_step_delay_ms")
            plant.torqueStepDelayMs = static_cast<uint32_t>(std::strtoul(rawValue.c_str(), nullptr, 10));
        else if (key == "response_sample_rate_hz")
            plant.responseSampleRateHz = static_cast<uint32_t>(std::strtoul(rawValue.c_str(), nullptr, 10));
        else if (key == "response_max_samples")
            plant.responseMaxSamples = static_cast<uint32_t>(std::strtoul(rawValue.c_str(), nullptr, 10));
        else if (key == "encoder_bias_radians")
            plant.encoderBiasRadians = value;
        else if (key == "over_current_trip_ampere")
            plant.overCurrentTripAmpere = value;
        else if (key == "over_voltage_trip_volts")
            plant.overVoltageTripVolts = value;
        else if (key == "under_voltage_trip_volts")
            plant.underVoltageTripVolts = value;
        else if (key == "over_temperature_trip_celsius")
            plant.overTemperatureTripCelsius = value;
        else if (key == "supply_voltage_scale")
            plant.supplyVoltageScale = value;
        else if (key == "random_seed")
            plant.randomSeed = static_cast<uint32_t>(std::strtoul(rawValue.c_str(), nullptr, 10));
        else
            return false;

        return true;
    }

    void RequireSimulatedTarget()
    {
        ASSERT_TRUE(TargetInteractor::Instance().SupportsSimulatedPlant())
            << "This scenario configures a simulated plant and cannot run against hardware";
    }
}

GIVEN(R"(a {word} motor plant)", (std::string preset))
{
    RequireSimulatedTarget();
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(sil::TryNamedPlant(preset, setup.plant)) << "Unknown motor plant preset: " << preset;
}

GIVEN(R"(an {word} phase motor plant)", (std::string phase))
{
    RequireSimulatedTarget();
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(sil::TryNamedPlant(phase + " phase", setup.plant))
        << "Unknown motor plant preset: " << phase << " phase";
}

GIVEN(R"(a motor plant with:)")
{
    RequireSimulatedTarget();
    auto& setup = context.Get<ScenarioSetup>();
    ASSERT_TRUE(dataTable.has_value()) << "This step needs a table of plant characteristics";

    for (const auto& row : dataTable->rows)
    {
        ASSERT_EQ(row.cells.size(), 2u) << "Each plant characteristic needs a name and a value";
        ASSERT_TRUE(ApplyPlantOverride(setup.plant, row.cells[0].value, row.cells[1].value))
            << "Unknown motor plant characteristic: " << row.cells[0].value;
    }
}

GIVEN(R"(the motor is already calibrated)")
{
    RequireSimulatedTarget();
    auto& setup = context.Get<ScenarioSetup>();
    setup.nvm.includeCalibration = true;
    setup.nvm.includeConfig = true;
    setup.nvm.calibration = sil::CompleteCalibration();
    setup.nvm.calibration.polePairs = setup.plant.polePairs;
    setup.nvm.calibration.rPhase = setup.plant.statorResistanceOhm;
    setup.nvm.calibration.lD = setup.plant.dAxisInductanceHenry;
    setup.nvm.calibration.lQ = setup.plant.qAxisInductanceHenry;
    setup.nvm.calibration.fluxLinkage = setup.plant.fluxLinkageWeber;
    setup.nvm.calibration.inertia = setup.plant.rotorInertiaKgM2;
    setup.nvm.calibration.frictionViscous = setup.plant.viscousDampingNmSPerRad;
}

WHEN(R"(the target boots)")
{
    auto& setup = context.Get<ScenarioSetup>();
    auto& interactor = TargetInteractor::Instance();

    interactor.ConfigurePlant(sil::EncodePlantConfig(setup.plant));
    interactor.ConfigureNonVolatileMemory(setup.nvm.includeCalibration || setup.nvm.includeConfig
                                              ? sil::BuildNvmImage(setup.nvm)
                                              : sil::BlankNvmImage());
    interactor.RestartTarget();
    setup.booted = true;

    auto& fixture = context.Get<Fixture>();
    ASSERT_TRUE(fixture.WaitForCanHeartbeat()) << "CAN stack not ready after boot";
}

GIVEN(R"(the stored calibration has a {word})", (std::string damage))
{
    RequireSimulatedTarget();
    auto& setup = context.Get<ScenarioSetup>();
    setup.nvm.includeCalibration = true;
    setup.nvm.calibration = sil::CompleteCalibration();

    if (damage == "corrupt")
        setup.nvm.calibrationDamage = sil::RecordDamage::corruptCrc;
    else if (damage == "wrong")
        setup.nvm.calibrationDamage = sil::RecordDamage::badMagic;
    else if (damage == "stale")
        setup.nvm.calibrationDamage = sil::RecordDamage::staleVersion;
    else
        FAIL() << "Unknown kind of damage: " << damage;
}

GIVEN(R"(the stored configuration has a {word})", (std::string damage))
{
    RequireSimulatedTarget();
    auto& setup = context.Get<ScenarioSetup>();
    setup.nvm.includeConfig = true;

    if (damage == "corrupt")
        setup.nvm.configDamage = sil::RecordDamage::corruptCrc;
    else if (damage == "wrong")
        setup.nvm.configDamage = sil::RecordDamage::badMagic;
    else if (damage == "stale")
        setup.nvm.configDamage = sil::RecordDamage::staleVersion;
    else
        FAIL() << "Unknown kind of damage: " << damage;
}
