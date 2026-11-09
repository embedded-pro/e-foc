#include "core/foc/model/drive/PlantDrive.hpp"
#include "core/foc/model/drive/SilPlantModel.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace sil
{
    namespace
    {
        constexpr uint32_t shortestSampleHoldClocks = 4;

        hal::Hertz BaseFrequencyOf(const std::optional<SilPlantConfig>& description, const PlantDriveDefaults& defaults)
        {
            if (!description || description->baseFrequencyHz == 0)
                return defaults.baseFrequency;

            return hal::Hertz{ description->baseFrequencyHz };
        }
    }

    PlantDrive::PlantDrive(const std::optional<SilPlantConfig>& description, const PlantDriveDefaults& defaults)
        : description{ description }
        , baseFrequency{ BaseFrequencyOf(description, defaults) }
        , model{ description ? MotorParameters(*description) : defaults.motor,
            description ? foc::Volts{ description->powerSupplyVoltageVolts } : defaults.supplyVoltage,
            baseFrequency }
    {
        if (description)
            Configure(*description);
    }

    void PlantDrive::Configure(const SilPlantConfig& plant)
    {
        ConfigureModel(model, plant);
        bootFaults = BootFaultInjection(plant);
        activeFaults = bootFaults;
        SampleAndHoldClocks(shortestSampleHoldClocks);

        if (plant.maxReadableDutyPercent != 0)
            maxReadableDuty = hal::DutyCycle::FromPercent(plant.maxReadableDutyPercent);
        if (plant.maxDutyPercent != 0)
            maxDuty = hal::DutyCycle::FromPercent(plant.maxDutyPercent);
        shuntReadout = foc::LowSideShuntReadout{ maxReadableDuty };
        dutyDelayed = plant.dutyDelayPeriods != 0;

        const uint32_t baseHz = baseFrequency.Value();
        if (plant.responseSampleRateHz != 0)
        {
            really_assert(baseHz % plant.responseSampleRateHz == 0);
            responseRecorder.Configure(baseHz / plant.responseSampleRateHz, plant.responseMaxSamples);
        }

        events.Configure(plant, baseHz);
    }

    void PlantDrive::SampleAndHoldClocks(uint32_t clocks)
    {
        if (description)
            model.CurrentSensor().SetChain(CurrentSense(*description, clocks));
    }

    void PlantDrive::Tick()
    {
        const uint32_t tick = tickCount.load(std::memory_order_relaxed) + 1;
        tickCount.store(tick, std::memory_order_relaxed);

        events.Fire(tick, model, activeFaults, responseRecorder);

        const auto& stepDuty = dutyDelayed ? appliedDutyPhases : lastDutyPhases;
        model.StepForTest(stepDuty);
        indexLatched = indexLatched || model.Encoder().IndexAsserted();
        shuntReadout.Applied(stepDuty);
        appliedDutyPhases = lastDutyPhases;
        responseRecorder.Capture(PlantSampleAt(model, tick));
    }

    uint32_t PlantDrive::TickCount() const
    {
        return tickCount.load(std::memory_order_relaxed);
    }

    foc::PhaseCurrents PlantDrive::ShuntCurrents() const
    {
        return shuntReadout.Read(sensor.Last());
    }

    std::optional<foc::PlantResponseRecord> PlantDrive::PopRecord()
    {
        return responseRecorder.Pop();
    }

    void PlantDrive::Output(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        lastDutyPhases = foc::KeepLowSideShuntsReadable(dutyPhases, maxReadableDuty, maxDuty);
    }

    void PlantDrive::Start()
    {
        model.Start();
        responseRecorder.Begin();
        events.Begin();
    }

    void PlantDrive::Stop()
    {
        model.Stop();
        model.Mechanics().SetExternalTorque(foc::NewtonMeter{ 0.0f });
        model.Mechanics().Grip(std::nullopt);
        activeFaults = bootFaults;
        foc::InjectFaults(model, activeFaults);
        responseRecorder.End();
    }

    void PlantDrive::CurrentOffsets(const foc::PhaseCurrents& offsets)
    {
        shuntReadout.Offsets(offsets);
    }

    hal::Hertz PlantDrive::BaseFrequency() const
    {
        return baseFrequency;
    }

    drivers::DeadTimeLoss PlantDrive::InverterDeadTimeLoss() const
    {
        if (!description)
            return {};

        return { std::chrono::nanoseconds{ static_cast<int64_t>(description->deadTimeCompensationNanoseconds + 0.5f) }, foc::Ampere{ description->deadTimeCompensationKneeAmpere } };
    }

    uint32_t PlantDrive::DutyUpdateDelayPeriods() const
    {
        return dutyDelayed ? 1 : 0;
    }

    void PlantDrive::ArmIndex()
    {
        indexLatched = false;
    }

    bool PlantDrive::IndexSeen() const
    {
        return indexLatched;
    }

    foc::ThreePhaseMotorModel& PlantDrive::Model()
    {
        return model;
    }
}
