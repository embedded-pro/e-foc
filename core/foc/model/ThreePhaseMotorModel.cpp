#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace foc
{
    namespace
    {
        constexpr float torqueConstant = 1.5f;
    }

    ThreePhaseMotorModel::ThreePhaseMotorModel(const Parameters& params, foc::Volts supplyVoltage, hal::Hertz pwmFrequency, std::optional<std::size_t> iterationLimit, bool selfDriveEnabled)
        : parameters(params)
        , baseFrequency(pwmFrequency)
        , selfDriveEnabled(selfDriveEnabled)
        , maxIterations(iterationLimit)
        , mechanical(params.J, params.B, params.p)
        , supply(supplyVoltage)
    {
    }

    void ThreePhaseMotorModel::SetLoad(foc::NewtonMeter load)
    {
        mechanical.SetLoad(load);
    }

    void ThreePhaseMotorModel::SetExternalTorque(foc::NewtonMeter torque)
    {
        mechanical.SetExternalTorque(torque);
    }

    foc::NewtonMeter ThreePhaseMotorModel::ExternalTorque() const
    {
        return mechanical.ExternalTorque();
    }

    foc::RadiansPerSecond ThreePhaseMotorModel::MechanicalSpeed() const
    {
        return mechanical.Speed();
    }

    foc::Radians ThreePhaseMotorModel::MechanicalAngle() const
    {
        return mechanical.Angle();
    }

    foc::RotatingFrame ThreePhaseMotorModel::LastDqCurrents() const
    {
        return electrical.LastDqCurrents();
    }

    void ThreePhaseMotorModel::SetAdcNoise(const NoiseConfig& config)
    {
        currentSensor.SetNoise(config);
    }

    void ThreePhaseMotorModel::SetEncoderNoise(const EncoderNoiseConfig& config)
    {
        encoder.SetNoise(config);
    }

    void ThreePhaseMotorModel::SetThermalConfig(const ThermalConfig& config)
    {
        thermal.Configure(config);
    }

    void ThreePhaseMotorModel::SetFaultInjection(const FaultInjectionConfig& config)
    {
        electrical.SetOpenPhases({ config.openPhaseA, config.openPhaseB, config.openPhaseC });
        encoder.SetStuck(config.encoderStuck, mechanical.Angle());
        supply.SetVoltageScale(config.supplyVoltageScale);
    }

    void ThreePhaseMotorModel::SetRandomSeed(uint32_t seed)
    {
        currentSensor.Seed(seed);
        encoder.Seed(seed ^ 0x9E3779B9u);
    }

    void ThreePhaseMotorModel::ResetTemperature()
    {
        thermal.Reset();
    }

    float ThreePhaseMotorModel::WindingTemperatureCelsius() const
    {
        return thermal.Temperature();
    }

    foc::PhaseCurrents ThreePhaseMotorModel::LastMeasuredCurrents() const
    {
        return currentSensor.Last();
    }

    foc::Volts ThreePhaseMotorModel::EffectiveSupplyVoltage() const
    {
        return supply.Voltage();
    }

    foc::Ampere ThreePhaseMotorModel::SupplyCurrent() const
    {
        return supply.Current();
    }

    void ThreePhaseMotorModel::SetSupply(const SupplyConfig& config)
    {
        supply.Configure(config);
    }

    void ThreePhaseMotorModel::SetCoulombFriction(foc::NewtonMeter friction)
    {
        mechanical.SetCoulombFriction(friction);
    }

    void ThreePhaseMotorModel::SetCogging(foc::NewtonMeter amplitude, uint32_t cyclesPerRevolution)
    {
        mechanical.SetCogging(amplitude, cyclesPerRevolution);
    }

    void ThreePhaseMotorModel::SetEncoderResolution(uint32_t countsPerRevolution)
    {
        encoder.SetResolution(countsPerRevolution);
    }

    foc::Ohm ThreePhaseMotorModel::EffectiveResistance() const
    {
        return thermal.Resistance(parameters.R);
    }

    foc::Henry ThreePhaseMotorModel::EffectiveInductanceD() const
    {
        return thermal.Inductance(parameters.Ld);
    }

    foc::Henry ThreePhaseMotorModel::EffectiveInductanceQ() const
    {
        return thermal.Inductance(parameters.Lq);
    }

    void ThreePhaseMotorModel::SetWindingTemperatureForTest(float celsius)
    {
        thermal.SetTemperature(celsius);
    }

    void ThreePhaseMotorModel::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        this->baseFrequency = baseFrequency;
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
        Model(dutyPhases);

        bool justFinished = false;
        if (counter.has_value())
        {
            const std::size_t remaining = --counter.value();
            justFinished = (remaining == 0);
            running = !justFinished;
        }

        currentSensor.Measure(dutyPhases, electrical.Currents(), electrical.PowerStage(), Period());
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
        mechanical.Reset();
        ResetTemperature();
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

    foc::Radians ThreePhaseMotorModel::Read()
    {
        return encoder.Read(mechanical.Angle());
    }

    void ThreePhaseMotorModel::Set(foc::Radians value)
    {
        mechanical.Set(value);
    }

    void ThreePhaseMotorModel::SetZero()
    {
        mechanical.SetZero();
    }

    void ThreePhaseMotorModel::SetPowerStage(const PowerStageConfig& config)
    {
        electrical.SetPowerStage(config);
    }

    void ThreePhaseMotorModel::SetCurrentSense(const CurrentSenseConfig& config)
    {
        currentSensor.SetChain(config);
    }

    float ThreePhaseMotorModel::Period() const
    {
        return 1.0f / static_cast<float>(baseFrequency.Value());
    }

    WindingParameters ThreePhaseMotorModel::Winding() const
    {
        return { EffectiveResistance().Value(), EffectiveInductanceD().Value(), EffectiveInductanceQ().Value(), parameters.psi_f.Value() };
    }

    void ThreePhaseMotorModel::Model(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        const auto dt = Period();
        const auto bus = supply.Voltage();
        lastPhaseVoltages = electrical.PhaseVoltages(dutyPhases, bus, dt);

        const auto winding = Winding();
        const auto iDqAtStart = electrical.Step(lastPhaseVoltages, mechanical.ElectricalAngle(), mechanical.ElectricalSpeed(), winding, dt);

        supply.Draw(electrical.InverterPower(lastPhaseVoltages), bus, dt);
        thermal.Step(electrical.CopperLoss(winding.resistance), dt);

        // The start-of-step torque keeps the position's second difference on the current reported with it.
        const auto torque = torqueConstant * parameters.p * (winding.fluxLinkage * iDqAtStart.q + (winding.inductanceD - winding.inductanceQ) * iDqAtStart.d * iDqAtStart.q);
        mechanical.Step(torque, dt);
    }
}
