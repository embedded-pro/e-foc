#pragma once

#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/model/drive/PlantEvents.hpp"
#include "core/foc/model/drive/SilPlantConfig.hpp"
#include "core/platform_abstraction/LowSideShuntWindow.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include <atomic>
#include <cstdint>
#include <optional>

namespace sil
{
    struct PlantDriveDefaults
    {
        foc::ThreePhaseMotorModel::Parameters motor;
        foc::Volts supplyVoltage;
        hal::Hertz baseFrequency;
    };

    class PlantDrive
    {
    public:
        PlantDrive(const std::optional<SilPlantConfig>& description, const PlantDriveDefaults& defaults);

        void SampleAndHoldClocks(uint32_t clocks);

        void Tick();
        uint32_t TickCount() const;
        foc::PhaseCurrents ShuntCurrents() const;
        std::optional<foc::PlantResponseRecord> PopRecord();

        void Output(const foc::PhasePwmDutyCycles& dutyPhases);
        void Start();
        void Stop();
        void CurrentOffsets(const foc::PhaseCurrents& offsets);
        hal::Hertz BaseFrequency() const;
        drivers::DeadTimeLoss InverterDeadTimeLoss() const;
        uint32_t DutyUpdateDelayPeriods() const;

        void ArmIndex();
        bool IndexSeen() const;

        foc::ThreePhaseMotorModel& Model();

    private:
        void Configure(const SilPlantConfig& plant);

        const std::optional<SilPlantConfig> description;
        const hal::Hertz baseFrequency;
        foc::ThreePhaseMotorModel model;
        const foc::PhaseCurrentSensor& sensor{ model.CurrentSensor() };
        PlantRecorder responseRecorder;
        PlantEvents events;
        foc::ThreePhaseMotorModel::FaultInjectionConfig bootFaults{};
        foc::ThreePhaseMotorModel::FaultInjectionConfig activeFaults{};

        // Written by the control tick, read from the event loop.
        std::atomic<uint32_t> tickCount{ 0 };
        bool indexLatched{ false };

        bool dutyDelayed{ false };
        hal::DutyCycle maxReadableDuty{ hal::DutyCycle::FromPercent(100) };
        hal::DutyCycle maxDuty{ hal::DutyCycle::FromPercent(100) };
        foc::LowSideShuntReadout shuntReadout{ maxReadableDuty };
        foc::PhasePwmDutyCycles lastDutyPhases{};
        foc::PhasePwmDutyCycles appliedDutyPhases{};
    };
}
