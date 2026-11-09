#include "integration_tests/model_in_the_loop/SimulatedDrive.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>

namespace mil
{
    namespace
    {
        const sil::PlantDriveDefaults defaults{
            .motor = {},
            .supplyVoltage = foc::Volts{ 0.0f },
            .baseFrequency = hal::Hertz{ 20000 },
        };

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
        : drive{ plant, defaults }
    {}

    void SimulatedDrive::ControlTick()
    {
        drive.Tick();

        if (onPhaseCurrentsReady.IsSet() && !controlLoopEntered)
        {
            controlLoopEntered = true;
            onPhaseCurrentsReady(drive.ShuntCurrents());
            controlLoopEntered = false;
        }
    }

    uint32_t SimulatedDrive::Tick() const
    {
        return drive.TickCount();
    }

    void SimulatedDrive::AppendRecords(std::vector<std::string>& lines, std::size_t maxRecords)
    {
        for (std::size_t i = 0; i != maxRecords; ++i)
        {
            const auto record = drive.PopRecord();
            if (!record)
                break;

            lines.push_back(Format(*record));
        }
    }

    foc::ThreePhaseMotorModel& SimulatedDrive::Model()
    {
        return drive.Model();
    }

    foc::Volts SimulatedDrive::PowerSupplyVoltage() const
    {
        return supply.Voltage();
    }

    drivers::DeadTimeLoss SimulatedDrive::InverterDeadTimeLoss() const
    {
        return drive.InverterDeadTimeLoss();
    }

    uint32_t SimulatedDrive::DutyUpdateDelayPeriods() const
    {
        return drive.DutyUpdateDelayPeriods();
    }

    void SimulatedDrive::PhaseCurrentsReady(const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        onPhaseCurrentsReady.Set(onDone);
    }

    void SimulatedDrive::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        drive.Output(dutyPhases);
    }

    void SimulatedDrive::Start()
    {
        drive.Start();
    }

    void SimulatedDrive::Stop()
    {
        drive.Stop();
    }

    hal::Hertz SimulatedDrive::BaseFrequency() const
    {
        return drive.BaseFrequency();
    }

    foc::Ampere SimulatedDrive::MaxCurrentSupported() const
    {
        return plantModel.MaxCurrentSupported();
    }

    void SimulatedDrive::CurrentOffsets(const foc::PhaseCurrents& offsets)
    {
        drive.CurrentOffsets(offsets);
    }

    foc::Radians SimulatedDrive::Read()
    {
        return drive.Model().Encoder().Read();
    }

    void SimulatedDrive::Set(foc::Radians value)
    {
        drive.Model().Encoder().Set(value);
    }

    void SimulatedDrive::SetZero()
    {
        drive.Model().Encoder().SetZero();
    }
}
