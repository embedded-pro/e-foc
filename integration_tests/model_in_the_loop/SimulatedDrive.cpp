#include "integration_tests/model_in_the_loop/SimulatedDrive.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "targets/platform_implementations/qemu/implementation/SilPlantModel.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>

namespace mil
{
    namespace
    {
        constexpr uint32_t shortestSampleHoldClocks = 4;
        constexpr uint32_t millisecondsPerSecond = 1000;

        hal::DutyCycle DutyOrFull(uint32_t percent)
        {
            return hal::DutyCycle::FromPercent(percent != 0 ? percent : 100);
        }

        long Milli(float value)
        {
            return std::lround(value * 1000.0f);
        }

        long Micro(float value)
        {
            return std::lround(value * 1000000.0f);
        }

        std::string Format(const foc::PlantResponseRecord& record)
        {
            const auto& sample = record.sample;
            const auto tick = static_cast<unsigned long>(sample.tick);
            std::array<char, 96> line{};

            switch (record.kind)
            {
                case foc::PlantResponseKind::sample:
                    std::snprintf(line.data(), line.size(), "PLANT %lu %ld %ld %ld %ld %ld", tick, Milli(sample.omegaMech), Micro(sample.thetaMech), Micro(sample.iq), Micro(sample.id), Micro(sample.externalTorqueNm));
                    break;
                case foc::PlantResponseKind::began:
                    std::snprintf(line.data(), line.size(), "PLANT_START %lu", tick);
                    break;
                case foc::PlantResponseKind::torqueStep:
                    std::snprintf(line.data(), line.size(), "PLANT_EVENT %lu torque %ld", tick, Micro(sample.externalTorqueNm));
                    break;
                case foc::PlantResponseKind::encoderFreeze:
                    std::snprintf(line.data(), line.size(), "PLANT_EVENT %lu encoder_freeze %ld", tick, Micro(sample.thetaMech));
                    break;
                case foc::PlantResponseKind::stopped:
                    std::snprintf(line.data(), line.size(), "PLANT_STOP %lu %lu", tick, static_cast<unsigned long>(record.dropped));
                    break;
            }

            return std::string{ line.data() };
        }
    }

    SimulatedDrive::SimulatedDrive(const sil::SilPlantConfig& plant)
        : plant{ plant }
        , baseFrequency{ plant.baseFrequencyHz != 0 ? plant.baseFrequencyHz : 20000u }
        , model{ sil::MotorParameters(plant), foc::Volts{ plant.powerSupplyVoltageVolts }, baseFrequency, std::nullopt, false }
        , bootFaultInjection{ sil::BootFaultInjection(plant) }
        , dutyDelayed{ plant.dutyDelayPeriods != 0 }
        , maxReadableDuty{ DutyOrFull(plant.maxReadableDutyPercent) }
        , maxDuty{ DutyOrFull(plant.maxDutyPercent) }
        , shuntReadout{ maxReadableDuty }
    {
        sil::ConfigureModel(model, plant);
        model.SetCurrentSense(sil::CurrentSense(plant, shortestSampleHoldClocks));

        const uint32_t baseHz = baseFrequency.Value();
        if (plant.responseSampleRateHz != 0)
        {
            really_assert(baseHz % plant.responseSampleRateHz == 0);
            responseRecorder.Configure(baseHz / plant.responseSampleRateHz, plant.responseMaxSamples);
        }

        torqueStepTorque = foc::NewtonMeter{ plant.torqueStepNm };
        const auto torqueStepDelay = static_cast<uint32_t>(static_cast<uint64_t>(plant.torqueStepDelayMs) * baseHz / millisecondsPerSecond);
        torqueStep.Configure(plant.torqueStepNm != 0.0f ? std::make_optional(torqueStepDelay) : std::nullopt);

        const auto encoderFreezeDelay = static_cast<uint32_t>(static_cast<uint64_t>(plant.encoderFreezeDelayCentiseconds) * 10u * baseHz / millisecondsPerSecond);
        encoderFreeze.Configure(encoderFreezeDelay != 0 ? std::make_optional(encoderFreezeDelay) : std::nullopt);
    }

    void SimulatedDrive::ControlTick()
    {
        const uint32_t tick = ++controlTick;

        if (torqueStep.Fire(tick))
        {
            model.SetExternalTorque(torqueStepTorque);
            responseRecorder.Note(foc::PlantResponseKind::torqueStep, SampleAt(tick));
        }

        if (encoderFreeze.Fire(tick))
        {
            auto frozen = bootFaultInjection;
            frozen.encoderStuck = true;
            model.SetFaultInjection(frozen);
            responseRecorder.Note(foc::PlantResponseKind::encoderFreeze, SampleAt(tick));
        }

        const auto& stepDuty = dutyDelayed ? appliedDutyPhases : lastDutyPhases;
        model.StepForTest(stepDuty);
        shuntReadout.Applied(stepDuty);
        appliedDutyPhases = lastDutyPhases;
        responseRecorder.Capture(SampleAt(tick));

        if (onPhaseCurrentsReady.IsSet() && !controlLoopEntered)
        {
            controlLoopEntered = true;
            onPhaseCurrentsReady(shuntReadout.Read(model.LastMeasuredCurrents()));
            controlLoopEntered = false;
        }
    }

    uint32_t SimulatedDrive::Tick() const
    {
        return controlTick;
    }

    void SimulatedDrive::AppendRecords(std::vector<std::string>& lines, std::size_t maxRecords)
    {
        for (std::size_t i = 0; i != maxRecords; ++i)
        {
            const auto record = responseRecorder.Pop();
            if (!record)
                break;

            lines.push_back(Format(*record));
        }
    }

    foc::ThreePhaseMotorModel& SimulatedDrive::Model()
    {
        return model;
    }

    foc::Volts SimulatedDrive::PowerSupplyVoltage() const
    {
        return model.EffectiveSupplyVoltage();
    }

    drivers::DeadTimeLoss SimulatedDrive::InverterDeadTimeLoss() const
    {
        return { std::chrono::nanoseconds{ static_cast<int64_t>(plant.deadTimeCompensationNanoseconds + 0.5f) }, foc::Ampere{ plant.deadTimeCompensationKneeAmpere } };
    }

    uint32_t SimulatedDrive::DutyUpdateDelayPeriods() const
    {
        return dutyDelayed ? 1 : 0;
    }

    void SimulatedDrive::PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        onPhaseCurrentsReady.Set(onDone);
    }

    void SimulatedDrive::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        lastDutyPhases = foc::KeepLowSideShuntsReadable(dutyPhases, maxReadableDuty, maxDuty);
    }

    void SimulatedDrive::Start()
    {
        model.Start();
        responseRecorder.Begin();
        torqueStep.Begin();
        encoderFreeze.Begin();
    }

    void SimulatedDrive::Stop()
    {
        model.Stop();
        model.SetExternalTorque(foc::NewtonMeter{ 0.0f });
        model.SetFaultInjection(bootFaultInjection);
        responseRecorder.End();
    }

    hal::Hertz SimulatedDrive::BaseFrequency() const
    {
        return baseFrequency;
    }

    foc::Ampere SimulatedDrive::MaxCurrentSupported() const
    {
        return model.MaxCurrentSupported();
    }

    foc::Radians SimulatedDrive::Read()
    {
        return model.Read();
    }

    void SimulatedDrive::Set(foc::Radians value)
    {
        model.Set(value);
    }

    void SimulatedDrive::SetZero()
    {
        model.SetZero();
    }

    foc::PlantResponseSample SimulatedDrive::SampleAt(uint32_t tick) const
    {
        const auto dq = model.LastDqCurrents();
        return foc::PlantResponseSample{ tick, model.MechanicalSpeed().Value(), model.MechanicalAngle().Value(), dq.q, dq.d, model.ExternalTorque().Value() };
    }
}
