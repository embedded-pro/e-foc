#pragma once

#include "core/foc/model/drive/PlantDrive.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace mil
{
    class SimulatedDrive
        : public drivers::ThreePhasePowerStage
        , public drivers::Encoder
    {
    public:
        explicit SimulatedDrive(const sil::SilPlantConfig& plant);

        void ControlTick();
        uint32_t Tick() const;
        void AppendRecords(std::vector<std::string>& lines, std::size_t maxRecords);

        foc::ThreePhaseMotorModel& Model();
        foc::Volts PowerSupplyVoltage() const;
        drivers::DeadTimeLoss InverterDeadTimeLoss() const;
        uint32_t DutyUpdateDelayPeriods() const;

        void PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;
        void CurrentOffsets(const foc::PhaseCurrents& offsets) override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        sil::PlantDrive drive;
        const foc::ThreePhaseMotorModel& plantModel{ drive.Model() };
        const foc::SupplyModel& supply{ drive.Model().Supply() };
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        bool controlLoopEntered{ false };
    };
}
