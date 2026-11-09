#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace foc
{
    namespace
    {
        constexpr float torqueConstant = 1.5f;

        struct Plant
        {
            ElectricalPlant& electrical;
            MechanicalPlant& mechanical;
            ThermalModel& thermal;
            SupplyModel& supply;
        };

        foc::ThreePhase StepPlant(const Plant& plant, const foc::PhasePwmDutyCycles& dutyPhases, const WindingParameters& winding, uint8_t polePairs, float dt)
        {
            const auto bus = plant.supply.Voltage();
            const auto phaseVoltages = plant.electrical.PhaseVoltages(dutyPhases, bus, dt);
            const auto iDqAtStart = plant.electrical.Step(phaseVoltages, plant.mechanical.ElectricalAngle(), plant.mechanical.ElectricalSpeed(), winding, dt);

            plant.supply.Draw(plant.electrical.InverterPower(phaseVoltages), bus, dt);
            plant.thermal.Step(plant.electrical.CopperLoss(winding.resistance), dt);

            const auto torque = torqueConstant * polePairs * (winding.fluxLinkage * iDqAtStart.q + (winding.inductanceD - winding.inductanceQ) * iDqAtStart.d * iDqAtStart.q);
            plant.mechanical.Step(torque, dt);
            return phaseVoltages;
        }
    }

    ThreePhaseMotorModel::ThreePhaseMotorModel(const Parameters& params, foc::Volts supplyVoltage, hal::Hertz pwmFrequency, std::optional<std::size_t> iterationLimit, bool selfDriveEnabled)
        : parameters(params)
        , baseFrequency(pwmFrequency)
        , period(1.0f / static_cast<float>(pwmFrequency.Value()))
        , selfDriveEnabled(selfDriveEnabled)
        , maxIterations(iterationLimit)
        , mechanical(params.J, params.B, params.p)
        , supply(supplyVoltage)
    {
    }

    MechanicalPlant& ThreePhaseMotorModel::Mechanics()
    {
        return mechanical;
    }

    ElectricalPlant& ThreePhaseMotorModel::Electrical()
    {
        return electrical;
    }

    ThermalModel& ThreePhaseMotorModel::Thermal()
    {
        return thermal;
    }

    SupplyModel& ThreePhaseMotorModel::Supply()
    {
        return supply;
    }

    PhaseCurrentSensor& ThreePhaseMotorModel::CurrentSensor()
    {
        return currentSensor;
    }

    SimulatedEncoder& ThreePhaseMotorModel::Encoder()
    {
        return encoder;
    }

    WindingParameters ThreePhaseMotorModel::Winding() const
    {
        return { thermal.Resistance(parameters.R).Value(), thermal.Inductance(parameters.Ld).Value(), thermal.Inductance(parameters.Lq).Value(), parameters.psi_f.Value() };
    }

    void ThreePhaseMotorModel::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        this->baseFrequency = baseFrequency;
        period = 1.0f / static_cast<float>(baseFrequency.Value());
        onCurrentPhasesReady = onDone;
    }

    void ThreePhaseMotorModel::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        selfDrive.pendingDuties = dutyPhases;
        EnableSelfDriving();
    }

    void ThreePhaseMotorModel::StepForTest(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        selfDrive.pendingDuties = dutyPhases;
        RunOneCycle(dutyPhases);
    }

    void ThreePhaseMotorModel::RunOneCycle(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        lastPhaseVoltages = StepPlant(Plant{ electrical, mechanical, thermal, supply }, dutyPhases, Winding(), parameters.p, period);

        bool justFinished = false;
        if (counter.has_value())
        {
            const std::size_t remaining = --counter.value();
            justFinished = (remaining == 0);
            running = !justFinished;
        }

        currentSensor.Measure(dutyPhases, electrical.Currents(), electrical.PowerStage(), period);
        encoder.Sample();

        const auto measured = currentSensor.Last();
        const auto vAbc = lastPhaseVoltages;
        const auto vAlphaBeta = clarke.Forward(vAbc);

        NotifyObservers([this, &measured, &vAbc, &vAlphaBeta](auto& observer)
            {
                observer.PhaseCurrentsWithMechanicalAngle(measured, mechanical.Angle(), mechanical.Speed());
                observer.StatorVoltages(vAbc, vAlphaBeta);
            });

        if (onCurrentPhasesReady && !justFinished)
            onCurrentPhasesReady(measured);

        if (justFinished)
        {
            selfDrive.driving = false;
            NotifyObservers([](auto& observer)
                {
                    observer.Finished();
                });
        }
    }

    void ThreePhaseMotorModel::EnableSelfDriving()
    {
        if (selfDrive.driving)
            return;

        selfDrive.driving = true;
        ScheduleNextCycle();
    }

    void ThreePhaseMotorModel::ScheduleNextCycle()
    {
        if (selfDrive.cycleScheduled || !selfDrive.driving)
            return;

        selfDrive.cycleScheduled = true;
        infra::EventDispatcherWithWeakPtr::Instance().Schedule(
            [](const infra::SharedPtr<ThreePhaseMotorModel>& self)
            {
                self->selfDrive.cycleScheduled = false;
                if (!self->selfDrive.driving)
                    return;
                self->RunOneCycle(self->selfDrive.pendingDuties);
                self->ScheduleNextCycle();
            },
            WeakFromThis());
    }

    void ThreePhaseMotorModel::Start()
    {
        if (running)
            return;

        running = true;
        counter = maxIterations;
        electrical.Reset();

        if (!encoder.HasIndex())
            mechanical.Reset();
        thermal.Reset();
        selfDrive.pendingDuties = foc::PhasePwmDutyCycles{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(49), hal::DutyCycle::FromPercent(51) };

        NotifyObservers([](auto& observer)
            {
                observer.Started();
            });

        if (selfDriveEnabled)
            EnableSelfDriving();
    }

    void ThreePhaseMotorModel::Stop()
    {
        running = false;
        selfDrive.driving = false;
        counter = std::nullopt;
        supply.Release();
        electrical.Reset();
        selfDrive.pendingDuties = foc::PhasePwmDutyCycles{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) };
        onCurrentPhasesReady = nullptr;
    }

    hal::Hertz ThreePhaseMotorModel::BaseFrequency() const
    {
        return baseFrequency;
    }

    foc::Ampere ThreePhaseMotorModel::MaxCurrentSupported() const
    {
        return parameters.maxSupportedCurrent;
    }

    void ThreePhaseMotorModel::CurrentOffsets(const foc::PhaseCurrents&)
    {}

    void InjectFaults(ThreePhaseMotorModel& model, const ThreePhaseMotorModel::FaultInjectionConfig& config)
    {
        model.Electrical().SetOpenPhases({ config.openPhaseA, config.openPhaseB, config.openPhaseC });
        model.Encoder().SetStuck(config.encoderStuck);
        model.Supply().SetVoltageScale(config.supplyVoltageScale);
    }

    void SeedNoise(ThreePhaseMotorModel& model, uint32_t seed)
    {
        model.CurrentSensor().Seed(seed);
        model.Encoder().Seed(seed ^ 0x9E3779B9u);
    }
}
