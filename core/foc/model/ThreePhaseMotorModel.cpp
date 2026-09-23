#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/DutyConversion.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <numbers>

namespace foc
{
    namespace
    {
        constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;
        constexpr float half = 0.5f;
        constexpr float torqueConstant = 1.5f;

    }

    ThreePhaseMotorModel::ThreePhaseMotorModel(const Parameters& params, foc::Volts supplyVoltage, hal::Hertz pwmFrequency, std::optional<std::size_t> iterationLimit, bool selfDriveEnabled)
        : parameters(params)
        , baseFrequency(pwmFrequency)
        , powerSupplyVoltage(supplyVoltage)
        , selfDriveEnabled(selfDriveEnabled)
        , maxIterations(iterationLimit)
    {
    }

    void ThreePhaseMotorModel::SetLoad(foc::NewtonMeter load)
    {
        this->load.emplace(load);
    }

    void ThreePhaseMotorModel::SetExternalTorque(foc::NewtonMeter torque)
    {
        externalTorque = torque;
    }

    foc::NewtonMeter ThreePhaseMotorModel::ExternalTorque() const
    {
        return externalTorque;
    }

    foc::RadiansPerSecond ThreePhaseMotorModel::MechanicalSpeed() const
    {
        return motorState.omega_mech;
    }

    foc::Radians ThreePhaseMotorModel::MechanicalAngle() const
    {
        return motorState.theta_mech;
    }

    foc::RotatingFrame ThreePhaseMotorModel::LastDqCurrents() const
    {
        return lastDqCurrents;
    }

    void ThreePhaseMotorModel::SetAdcNoise(const NoiseConfig& config)
    {
        currentNoise.config = config;
    }

    void ThreePhaseMotorModel::SetEncoderNoise(const EncoderNoiseConfig& config)
    {
        encoderNoise.config = config;
    }

    void ThreePhaseMotorModel::SetThermalConfig(const ThermalConfig& config)
    {
        thermal.config = config;
    }

    void ThreePhaseMotorModel::SetFaultInjection(const FaultInjectionConfig& config)
    {
        const bool becameStuck = config.encoderStuck && !faultInjection.config.encoderStuck;
        faultInjection.config = config;

        if (becameStuck)
            faultInjection.stuckAngle = motorState.theta_mech;
    }

    void ThreePhaseMotorModel::SetRandomSeed(uint32_t seed)
    {
        currentNoise.engine.seed(seed);
        encoderNoise.engine.seed(seed ^ 0x9E3779B9u);
    }

    void ThreePhaseMotorModel::ResetTemperature()
    {
        thermal.windingTempCelsius = thermal.config.ambientCelsius;
    }

    float ThreePhaseMotorModel::WindingTemperatureCelsius() const
    {
        return thermal.windingTempCelsius;
    }

    foc::PhaseCurrents ThreePhaseMotorModel::LastMeasuredCurrents() const
    {
        return { currentNoise.iaLast, currentNoise.ibLast, currentNoise.icLast };
    }

    foc::Volts ThreePhaseMotorModel::EffectiveSupplyVoltage() const
    {
        return foc::Volts{ powerSupplyVoltage.Value() * faultInjection.config.supplyVoltageScale };
    }

    void ThreePhaseMotorModel::ApplyOpenPhases()
    {
        if (faultInjection.config.openPhaseA)
            motorState.ia = foc::Ampere{ 0.0f };
        if (faultInjection.config.openPhaseB)
            motorState.ib = foc::Ampere{ 0.0f };
        if (faultInjection.config.openPhaseC)
            motorState.ic = foc::Ampere{ 0.0f };
    }

    foc::Ohm ThreePhaseMotorModel::EffectiveResistance() const
    {
        const float deltaT = thermal.windingTempCelsius - thermal.config.referenceCelsius;
        return foc::Ohm{ parameters.R.Value() * (1.0f + thermal.config.copperTempCoeff * deltaT) };
    }

    foc::Henry ThreePhaseMotorModel::EffectiveInductanceD() const
    {
        const float deltaT = thermal.windingTempCelsius - thermal.config.referenceCelsius;
        return foc::Henry{ parameters.Ld.Value() * (1.0f - thermal.config.ironInductanceCoeff * deltaT) };
    }

    foc::Henry ThreePhaseMotorModel::EffectiveInductanceQ() const
    {
        const float deltaT = thermal.windingTempCelsius - thermal.config.referenceCelsius;
        return foc::Henry{ parameters.Lq.Value() * (1.0f - thermal.config.ironInductanceCoeff * deltaT) };
    }

    void ThreePhaseMotorModel::SetWindingTemperatureForTest(float celsius)
    {
        thermal.windingTempCelsius = celsius;
    }

    float ThreePhaseMotorModel::SampleNoise()
    {
        return currentNoise.config.sigmaAmpere * currentNoise.distribution(currentNoise.engine);
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

        const auto iaNoise = foc::Ampere{ motorState.ia.Value() + currentNoise.config.biasAmpereA + SampleNoise() };
        const auto ibNoise = foc::Ampere{ motorState.ib.Value() + currentNoise.config.biasAmpereB + SampleNoise() };
        const auto icNoise = foc::Ampere{ motorState.ic.Value() + currentNoise.config.biasAmpereC + SampleNoise() };
        currentNoise.iaLast = iaNoise;
        currentNoise.ibLast = ibNoise;
        currentNoise.icLast = icNoise;

        const auto supply = EffectiveSupplyVoltage().Value();
        const auto va = (foc::DutyFraction(dutyPhases.a) - half) * supply;
        const auto vb = (foc::DutyFraction(dutyPhases.b) - half) * supply;
        const auto vc = (foc::DutyFraction(dutyPhases.c) - half) * supply;
        const foc::ThreePhase vAbc{ va, vb, vc };
        const auto vAlphaBeta = clarke.Forward(vAbc);

        NotifyObservers([this, &vAbc, &vAlphaBeta](auto& observer)
            {
                observer.PhaseCurrentsWithMechanicalAngle({ currentNoise.iaLast, currentNoise.ibLast, currentNoise.icLast }, motorState.theta_mech, motorState.omega_mech);
                observer.StatorVoltages(vAbc, vAlphaBeta);
            });

        if (onCurrentPhasesReady && !justFinished)
            onCurrentPhasesReady({ currentNoise.iaLast, currentNoise.ibLast, currentNoise.icLast });

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
        motorState.ia = foc::Ampere{ 0.0f };
        motorState.ib = foc::Ampere{ 0.0f };
        motorState.ic = foc::Ampere{ 0.0f };
        motorState.theta = foc::Radians{ 0.0f };
        motorState.theta_mech = foc::Radians{ 0.0f };
        motorState.omega = foc::RadiansPerSecond{ 0.0f };
        motorState.omega_mech = foc::RadiansPerSecond{ 0.0f };
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
        motorState.ia = foc::Ampere{ 0.0f };
        motorState.ib = foc::Ampere{ 0.0f };
        motorState.ic = foc::Ampere{ 0.0f };
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
        const auto angle = faultInjection.config.encoderStuck ? faultInjection.stuckAngle : motorState.theta_mech;
        const float noise = encoderNoise.config.sigmaRadians * encoderNoise.distribution(encoderNoise.engine);
        return foc::Radians{ detail::PositionWithWrapAround(angle.Value() + encoderNoise.config.biasRadians + noise) };
    }

    void ThreePhaseMotorModel::Set(foc::Radians value)
    {
        motorState.theta_mech = value;
        motorState.theta = foc::Radians{ parameters.p * motorState.theta_mech.Value() };
    }

    void ThreePhaseMotorModel::SetZero()
    {
        motorState.theta_mech = foc::Radians{ 0.0f };
        motorState.theta = foc::Radians{ 0.0f };
    }

    void ThreePhaseMotorModel::Model(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        auto dt = 1.0f / static_cast<float>(baseFrequency.Value());
        auto duty_a = foc::DutyFraction(dutyPhases.a);
        auto duty_b = foc::DutyFraction(dutyPhases.b);
        auto duty_c = foc::DutyFraction(dutyPhases.c);

        const auto supply = EffectiveSupplyVoltage();
        auto va = (duty_a - half) * supply;
        auto vb = (duty_b - half) * supply;
        auto vc = (duty_c - half) * supply;

        // The dq equations below already carry the coupling the rotating frame produces, so the currents they
        // integrate are expressed in the frame at the end of the step and go back to abc at that angle; converting
        // them at the starting angle and re-reading them at the next one counted the frame rotation twice. The
        // inverter's voltage is fixed in the stator for the whole step, so the rotor sees it at the mid-step angle.
        const auto stepRotation = motorState.omega.Value() * dt;
        auto cos_theta = foc::FastTrigonometry::Cosine(motorState.theta.Value());
        auto sin_theta = foc::FastTrigonometry::Sine(motorState.theta.Value());
        const auto midAngle = motorState.theta.Value() + 0.5f * stepRotation;
        const auto endAngle = motorState.theta.Value() + stepRotation;
        const auto cos_end = foc::FastTrigonometry::Cosine(endAngle);
        const auto sin_end = foc::FastTrigonometry::Sine(endAngle);

        auto v_dq = park.Forward(clarke.Forward(foc::ThreePhase{ va.Value(), vb.Value(), vc.Value() }), foc::FastTrigonometry::Cosine(midAngle), foc::FastTrigonometry::Sine(midAngle));
        auto i_dq = park.Forward(clarke.Forward(foc::ThreePhase{ motorState.ia.Value(), motorState.ib.Value(), motorState.ic.Value() }), cos_theta, sin_theta);

        auto id = i_dq.d;
        auto iq = i_dq.q;

        const auto rEff = EffectiveResistance().Value();
        const auto ldEff = EffectiveInductanceD().Value();
        const auto lqEff = EffectiveInductanceQ().Value();

        auto dId_dt = (v_dq.d - rEff * id + motorState.omega.Value() * lqEff * iq) / ldEff;
        auto dIq_dt = (v_dq.q - rEff * iq - motorState.omega.Value() * ldEff * id - motorState.omega.Value() * parameters.psi_f.Value()) / lqEff;

        id += dId_dt * dt;
        iq += dIq_dt * dt;

        auto i_abc = clarke.Inverse(park.Inverse(foc::RotatingFrame{ id, iq }, cos_end, sin_end));

        motorState.ia = foc::Ampere{ i_abc.a };
        motorState.ib = foc::Ampere{ i_abc.b };
        motorState.ic = foc::Ampere{ i_abc.c };

        ApplyOpenPhases();

        if (faultInjection.config.AnyPhaseOpen())
        {
            const auto masked = park.Forward(clarke.Forward(foc::ThreePhase{ motorState.ia.Value(), motorState.ib.Value(), motorState.ic.Value() }), cos_end, sin_end);
            id = masked.d;
            iq = masked.q;
        }

        const auto pCu = rEff * (motorState.ia.Value() * motorState.ia.Value() + motorState.ib.Value() * motorState.ib.Value() + motorState.ic.Value() * motorState.ic.Value());
        thermal.windingTempCelsius += dt * (pCu - (thermal.windingTempCelsius - thermal.config.ambientCelsius) / thermal.config.thermalResistance) / thermal.config.thermalCapacitance;

        auto torqueElec = torqueConstant * parameters.p * (parameters.psi_f.Value() * iq + (ldEff - lqEff) * id * iq);

        auto loadTorque = load.value_or(foc::NewtonMeter{ 0.0f }).Value();
        auto loadOpposing = (motorState.omega_mech.Value() >= 0.0f) ? loadTorque : -loadTorque;

        lastDqCurrents = foc::RotatingFrame{ id, iq };

        auto d_omega_mech_dt = (torqueElec - parameters.B.Value() * motorState.omega_mech.Value() - loadOpposing - externalTorque.Value()) / parameters.J.Value();

        motorState.omega_mech += foc::RadiansPerSecond{ d_omega_mech_dt * dt };
        motorState.omega = foc::RadiansPerSecond{ parameters.p * motorState.omega_mech.Value() };

        motorState.theta_mech += foc::Radians{ motorState.omega_mech.Value() * dt };
        motorState.theta += foc::Radians{ motorState.omega.Value() * dt };

        motorState.theta_mech = foc::Radians{ detail::PositionWithWrapAround(motorState.theta_mech.Value()) };
        motorState.theta = foc::Radians{ detail::PositionWithWrapAround(motorState.theta.Value()) };
    }
}
