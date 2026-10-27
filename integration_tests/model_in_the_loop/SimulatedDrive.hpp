#pragma once

#include "core/foc/math/LowSideShuntWindow.hpp"
#include "core/foc/model/EncoderFreezeScheduler.hpp"
#include "core/foc/model/PlantResponseRecorder.hpp"
#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/model/TorqueStepScheduler.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/util/HotSwapFunction.hpp"
#include "targets/platform_implementations/qemu/implementation/SilPlantConfig.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace mil
{
    class SimulatedDrive
        : public drivers::ThreePhaseInverter
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

        void PhaseCurrentsReady(hal::Hertz frequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        foc::PlantResponseSample SampleAt(uint32_t tick) const;

        const sil::SilPlantConfig plant;
        const hal::Hertz baseFrequency;
        foc::ThreePhaseMotorModel model;
        foc::PlantResponseRecorder<1024> responseRecorder;
        foc::TorqueStepScheduler torqueStep;
        foc::EncoderFreezeScheduler encoderFreeze;
        foc::ThreePhaseMotorModel::FaultInjectionConfig bootFaultInjection;
        infra::HotSwapFunction<void(foc::PhaseCurrents)> onPhaseCurrentsReady;
        uint32_t controlTick{ 0 };
        uint32_t sampleDecimation{ 1 };
        uint32_t samplePhase{ 0 };
        bool controlLoopEntered{ false };
        bool dutyDelayed;
        hal::DutyCycle maxReadableDuty;
        hal::DutyCycle maxDuty;
        foc::LowSideShuntReadout shuntReadout;
        foc::PhasePwmDutyCycles lastDutyPhases{};
        foc::PhasePwmDutyCycles appliedDutyPhases{};
    };
}
