#pragma once

#include "core/foc/model/drive/BoardProtectionSimulator.hpp"
#include "core/foc/model/drive/PlantDrive.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"

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

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

        bool HasIndex() const override;
        void ArmIndex() override;
        bool IndexSeen() override;

    protected:
        ~PlantPowerStage() = default;

        void StepPlant();
        void DeliverPendingProtection();

    private:
        sil::PlantDrive drive;
        const foc::ThreePhaseMotorModel& plantModel{ drive.Model() };
        const foc::SimulatedEncoder& plantEncoder{ drive.Model().Encoder() };
        application::BoardProtectionSimulator boardProtection;
        infra::Function<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        bool controlLoopEntered{ false };
    };
}
