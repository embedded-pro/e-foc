#pragma once

#include "core/foc/model/drive/BoardProtectionSimulator.hpp"
#include "core/foc/model/drive/PlantDrive.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "tools/virtual_motor/platform/DrivenShaft.hpp"
#include "tools/virtual_motor/platform/PlantDisturbances.hpp"
#include "tools/virtual_motor/platform/PlantSignals.hpp"

namespace virtual_motor
{
    class PlantPowerStage
        : public application::PlatformFactory
        , public drivers::EncoderIndex
    {
    public:
        explicit PlantPowerStage(const sil::SilPlantConfig& plant);

        void ConfigureAdcAndPwm(hal::Hertz baseFrequency, std::chrono::nanoseconds deadTime, SampleAndHold sampleAndHold) override;
        drivers::DeadTimeLoss InverterDeadTimeLoss(std::chrono::nanoseconds deadTime) const override;
        uint32_t DutyUpdateDelayPeriods() const override;
        void SetEncoderResolution(uint32_t resolution) override;
        foc::Volts PowerSupplyVoltage() override;
        void RegisterBoardProtection(const infra::Function<void(BoardProtectionReason)>& onProtection) override;
        BoardProtectionState BoardProtectionStatus() override;
        std::optional<std::reference_wrapper<drivers::EncoderIndex>> EncoderIndexChannel() override;

        void PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;
        void CurrentOffsets(const foc::PhaseCurrents& offsets) override;
        void ZeroVectorSlopes(const foc::PhaseCurrentSlopes& slopes) override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

        bool HasIndex() const override;
        void ArmIndex() override;
        bool IndexSeen() override;

        PlantDisturbances& Disturbances();
        SignalSample SamplePlant();

    protected:
        ~PlantPowerStage() = default;

        void StepPlant();
        void DeliverPendingProtection();

    private:
        void DriveShaft();

        class ModelInputs
            : public DisturbanceInputs
        {
        public:
            ModelInputs(foc::ThreePhaseMotorModel& model, const sil::SilPlantConfig& plant, DrivenShaft& shaft);

            void SetLoadTorque(foc::NewtonMeter torque) override;
            void SetExternalTorque(foc::NewtonMeter torque) override;
            void SetCurrentNoise(foc::Ampere sigma) override;
            void SetEncoderNoise(foc::Radians sigma) override;
            void SetShaftDrive(const std::optional<ShaftDrive>& drive) override;

        private:
            foc::ThreePhaseMotorModel& model;
            const sil::SilPlantConfig& plant;
            DrivenShaft& shaft;
        };

        sil::PlantDrive drive;
        const foc::ThreePhaseMotorModel& plantModel{ drive.Model() };
        const foc::SimulatedEncoder& plantEncoder{ drive.Model().Encoder() };
        application::BoardProtectionSimulator boardProtection;
        sil::SilPlantConfig plant;
        DrivenShaft shaft{ 1.0f / static_cast<float>(drive.BaseFrequency().Value()) };
        bool shaftDriven{ false };
        ModelInputs modelInputs{ drive.Model(), plant, shaft };
        PlantDisturbances disturbances;
        PlantSignals signals;

        infra::Function<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        bool controlLoopEntered{ false };
    };
}
