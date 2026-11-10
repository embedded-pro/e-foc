#include "tools/virtual_motor/platform/PlantPowerStage.hpp"
#include "motor_parameters/TeknicM2310pLn04k.hpp"
#include <array>

namespace virtual_motor
{
    namespace
    {
        constexpr uint32_t defaultBaseFrequencyHz{ 20000 };
        constexpr std::array<uint32_t, 5> sampleHoldClocks{ 4, 16, 32, 64, 256 };

        sil::PlantDriveDefaults Defaults()
        {
            return { foc::M_2310P_LN_04K::parameters, foc::M_2310P_LN_04K::ratedSupply, hal::Hertz{ defaultBaseFrequencyHz } };
        }
    }

    PlantPowerStage::PlantPowerStage(const sil::SilPlantConfig& plant)
        : drive(plant, Defaults())
        , plant(plant)
        , disturbances(modelInputs, { foc::NewtonMeter{ plant.loadTorqueNm }, foc::Ampere{ plant.noiseSigmaAmpere }, foc::Radians{ plant.encoderSigmaRadians } }, drive.BaseFrequency().Value())
    {
        boardProtection.Configure({
            .overCurrentAmpere = plant.overCurrentTripAmpere,
            .overVoltageVolts = plant.overVoltageTripVolts,
            .underVoltageVolts = plant.underVoltageTripVolts,
            .overTemperatureCelsius = plant.overTemperatureTripCelsius,
        });
    }

    void PlantPowerStage::StepPlant()
    {
        disturbances.Apply();
        drive.Tick();

        if (onPhaseCurrentsReady && !controlLoopEntered)
        {
            controlLoopEntered = true;
            onPhaseCurrentsReady(drive.ShuntCurrents());
            controlLoopEntered = false;
        }

        auto& model = drive.Model();
        boardProtection.Evaluate(model.CurrentSensor().Last(), model.Supply().Voltage().Value(), model.Thermal().Temperature());
    }

    PlantDisturbances& PlantPowerStage::Disturbances()
    {
        return disturbances;
    }

    void PlantPowerStage::DeliverPendingProtection()
    {
        boardProtection.DeliverPendingProtection();
    }

    void PlantPowerStage::ConfigureAdcAndPwm(hal::Hertz, std::chrono::nanoseconds, SampleAndHold sampleAndHold)
    {
        drive.SampleAndHoldClocks(sampleHoldClocks.at(static_cast<std::size_t>(sampleAndHold)));
    }

    drivers::DeadTimeLoss PlantPowerStage::InverterDeadTimeLoss(std::chrono::nanoseconds) const
    {
        return drive.InverterDeadTimeLoss();
    }

    uint32_t PlantPowerStage::DutyUpdateDelayPeriods() const
    {
        return drive.DutyUpdateDelayPeriods();
    }

    void PlantPowerStage::SetEncoderResolution(uint32_t)
    {}

    foc::Volts PlantPowerStage::PowerSupplyVoltage()
    {
        return drive.Model().Supply().Voltage();
    }

    void PlantPowerStage::RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>& onProtection)
    {
        boardProtection.Register(onProtection);
    }

    application::PlatformFactory::BoardProtectionState PlantPowerStage::BoardProtectionStatus()
    {
        return boardProtection.Status();
    }

    std::optional<std::reference_wrapper<drivers::EncoderIndex>> PlantPowerStage::EncoderIndexChannel()
    {
        return *this;
    }

    void PlantPowerStage::PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone)
    {
        onPhaseCurrentsReady = onDone;
    }

    void PlantPowerStage::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        drive.Output(dutyPhases);
    }

    void PlantPowerStage::Start()
    {
        drive.Start();
        boardProtection.SetArmed(true);
    }

    void PlantPowerStage::Stop()
    {
        boardProtection.SetArmed(false);
        drive.Stop();
    }

    hal::Hertz PlantPowerStage::BaseFrequency() const
    {
        return drive.BaseFrequency();
    }

    foc::Ampere PlantPowerStage::MaxCurrentSupported() const
    {
        return plantModel.MaxCurrentSupported();
    }

    void PlantPowerStage::CurrentOffsets(const foc::PhaseCurrents& offsets)
    {
        drive.CurrentOffsets(offsets);
    }

    foc::Radians PlantPowerStage::Read()
    {
        return drive.Model().Encoder().Read();
    }

    void PlantPowerStage::Set(foc::Radians value)
    {
        drive.Model().Encoder().Set(value);
    }

    void PlantPowerStage::SetZero()
    {
        drive.Model().Encoder().SetZero();
    }

    bool PlantPowerStage::HasIndex() const
    {
        return plantEncoder.HasIndex();
    }

    void PlantPowerStage::ArmIndex()
    {
        drive.ArmIndex();
    }

    bool PlantPowerStage::IndexSeen()
    {
        return drive.IndexSeen();
    }

    PlantPowerStage::ModelInputs::ModelInputs(foc::ThreePhaseMotorModel& model, const sil::SilPlantConfig& plant)
        : model(model)
        , plant(plant)
    {}

    void PlantPowerStage::ModelInputs::SetLoadTorque(foc::NewtonMeter torque)
    {
        model.Mechanics().SetLoad(torque);
    }

    void PlantPowerStage::ModelInputs::SetExternalTorque(foc::NewtonMeter torque)
    {
        model.Mechanics().SetExternalTorque(torque);
    }

    void PlantPowerStage::ModelInputs::SetCurrentNoise(foc::Ampere sigma)
    {
        model.CurrentSensor().SetNoise({
            .sigmaAmpere = sigma.Value(),
            .biasAmpereA = plant.noiseBiasAmpereA,
            .biasAmpereB = plant.noiseBiasAmpereB,
            .biasAmpereC = plant.noiseBiasAmpereC,
        });
    }

    void PlantPowerStage::ModelInputs::SetEncoderNoise(foc::Radians sigma)
    {
        model.Encoder().SetNoise({
            .sigmaRadians = sigma.Value(),
            .biasRadians = plant.encoderBiasRadians,
        });
    }
}
