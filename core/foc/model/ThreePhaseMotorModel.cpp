#include "core/foc/model/ThreePhaseMotorModel.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/math/AngleWrap.hpp"
#include "core/foc/math/DutyConversion.hpp"
#include "core/foc/math/FastTrigonometry.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace foc
{
    namespace
    {
        constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;
        constexpr float half = 0.5f;
        constexpr float torqueConstant = 1.5f;
        constexpr float supplyBulkTimeConstantSeconds = 0.001f;
        constexpr float minimumBusVoltage = 1.0f;

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
        loads.load.emplace(load);
    }

    void ThreePhaseMotorModel::SetExternalTorque(foc::NewtonMeter torque)
    {
        loads.externalTorque = torque;
    }

    foc::NewtonMeter ThreePhaseMotorModel::ExternalTorque() const
    {
        return loads.externalTorque;
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
        encoderNoise.lastSample = 0.0f;
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
        const float open = powerSupplyVoltage.Value() * faultInjection.config.supplyVoltageScale;
        float loaded = open - supplyState.config.sourceResistanceOhm * supplyState.busCurrentAmpere;

        if (supplyState.config.currentLimitAmpere > 0.0f && supplyState.busCurrentAmpere > supplyState.config.currentLimitAmpere)
            loaded *= supplyState.config.currentLimitAmpere / supplyState.busCurrentAmpere;

        return foc::Volts{ std::max(loaded, 0.0f) };
    }

    foc::Ampere ThreePhaseMotorModel::SupplyCurrent() const
    {
        return foc::Ampere{ supplyState.busCurrentAmpere };
    }

    void ThreePhaseMotorModel::SetSupply(const SupplyConfig& config)
    {
        supplyState.config = config;
        supplyState.busCurrentAmpere = 0.0f;
    }

    void ThreePhaseMotorModel::SetCoulombFriction(foc::NewtonMeter friction)
    {
        nonIdealities.coulombFrictionNm = friction.Value();
    }

    void ThreePhaseMotorModel::SetCogging(foc::NewtonMeter amplitude, uint32_t cyclesPerRevolution)
    {
        nonIdealities.coggingTorqueNm = amplitude.Value();
        nonIdealities.coggingCyclesPerRevolution = cyclesPerRevolution;
    }

    void ThreePhaseMotorModel::SetEncoderResolution(uint32_t countsPerRevolution)
    {
        nonIdealities.encoderCountsPerRevolution = countsPerRevolution;
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

    float ThreePhaseMotorModel::SampleEncoderNoise()
    {
        return encoderNoise.config.sigmaRadians * encoderNoise.distribution(encoderNoise.engine);
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

        const auto measured = MeasuredCurrents(dutyPhases);
        currentNoise.iaLast = measured.a;
        currentNoise.ibLast = measured.b;
        currentNoise.icLast = measured.c;
        encoderNoise.lastSample = SampleEncoderNoise();

        const auto vAbc = lastPhaseVoltages;
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
        supplyState.busCurrentAmpere = 0.0f;
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
        float measured = angle.Value() + encoderNoise.config.biasRadians + encoderNoise.lastSample;

        if (nonIdealities.encoderCountsPerRevolution != 0)
        {
            const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(nonIdealities.encoderCountsPerRevolution);
            measured = std::floor(measured / step) * step;
        }

        return foc::Radians{ detail::PositionWithWrapAround(measured) };
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

    void ThreePhaseMotorModel::SetPowerStage(const PowerStageConfig& config)
    {
        powerStage = config;
    }

    void ThreePhaseMotorModel::SetCurrentSense(const CurrentSenseConfig& config)
    {
        currentSense = config;
    }

    foc::PhaseCurrents ThreePhaseMotorModel::MeasuredCurrents(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        const auto& noise = currentNoise.config;

        if (!CurrentSenseEnabled(currentSense))
            return { foc::Ampere{ motorState.ia.Value() + noise.biasAmpereA + SampleNoise() },
                foc::Ampere{ motorState.ib.Value() + noise.biasAmpereB + SampleNoise() },
                foc::Ampere{ motorState.ic.Value() + noise.biasAmpereC + SampleNoise() } };

        const auto period = 1.0f / static_cast<float>(baseFrequency.Value());
        const infra::Function<float()> conversionNoise{ [this]()
            {
                return SampleNoise();
            } };
        const auto read = [this, period, &conversionNoise](uint32_t step, hal::DutyCycle duty, foc::Ampere current, float offset)
        {
            const ShuntSample sample{ LowSideConduction(powerStage, foc::DutyFraction(duty), current.Value(), period), current.Value(), offset };
            return foc::Ampere{ ShuntReading(currentSense, sample, step, conversionNoise) };
        };

        return { read(0, dutyPhases.a, motorState.ia, noise.biasAmpereA), read(1, dutyPhases.b, motorState.ib, noise.biasAmpereB), read(2, dutyPhases.c, motorState.ic, noise.biasAmpereC) };
    }

    foc::ThreePhase ThreePhaseMotorModel::PhaseVoltages(const foc::PhasePwmDutyCycles& dutyPhases, foc::Volts supply) const
    {
        const auto period = 1.0f / static_cast<float>(baseFrequency.Value());
        const auto bus = supply.Value();
        const auto leg = [this, period, bus](hal::DutyCycle duty, foc::Ampere current)
        {
            return PowerStageLegVoltage(powerStage, foc::DutyFraction(duty), current.Value(), bus, period) - half * bus;
        };

        return foc::ThreePhase{ leg(dutyPhases.a, motorState.ia), leg(dutyPhases.b, motorState.ib), leg(dutyPhases.c, motorState.ic) };
    }

    void ThreePhaseMotorModel::Model(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        auto dt = 1.0f / static_cast<float>(baseFrequency.Value());
        const auto supply = EffectiveSupplyVoltage();
        lastPhaseVoltages = PhaseVoltages(dutyPhases, supply);
        const auto va = foc::Volts{ lastPhaseVoltages.a };
        const auto vb = foc::Volts{ lastPhaseVoltages.b };
        const auto vc = foc::Volts{ lastPhaseVoltages.c };

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

        if (supplyState.config.sourceResistanceOhm > 0.0f || supplyState.config.currentLimitAmpere > 0.0f)
        {
            const float inputPower = va.Value() * motorState.ia.Value() + vb.Value() * motorState.ib.Value() + vc.Value() * motorState.ic.Value();
            const float instantaneous = std::max(inputPower, 0.0f) / std::max(supply.Value(), minimumBusVoltage);
            supplyState.busCurrentAmpere += (instantaneous - supplyState.busCurrentAmpere) * std::min(dt / supplyBulkTimeConstantSeconds, 1.0f);
        }

        if (faultInjection.config.AnyPhaseOpen())
        {
            const auto masked = park.Forward(clarke.Forward(foc::ThreePhase{ motorState.ia.Value(), motorState.ib.Value(), motorState.ic.Value() }), cos_end, sin_end);
            id = masked.d;
            iq = masked.q;
        }

        const auto pCu = rEff * (motorState.ia.Value() * motorState.ia.Value() + motorState.ib.Value() * motorState.ib.Value() + motorState.ic.Value() * motorState.ic.Value());
        thermal.windingTempCelsius += dt * (pCu - (thermal.windingTempCelsius - thermal.config.ambientCelsius) / thermal.config.thermalResistance) / thermal.config.thermalCapacitance;

        // The start-of-step torque keeps the position's second difference on the current reported with it.
        auto torqueElec = torqueConstant * parameters.p * (parameters.psi_f.Value() * i_dq.q + (ldEff - lqEff) * i_dq.d * i_dq.q);

        auto loadTorque = loads.load.value_or(foc::NewtonMeter{ 0.0f }).Value();
        auto loadOpposing = (motorState.omega_mech.Value() >= 0.0f) ? loadTorque : -loadTorque;

        lastDqCurrents = foc::RotatingFrame{ id, iq };

        const auto cogging = nonIdealities.coggingTorqueNm != 0.0f
                                 ? nonIdealities.coggingTorqueNm * foc::FastTrigonometry::Sine(static_cast<float>(nonIdealities.coggingCyclesPerRevolution) * motorState.theta_mech.Value())
                                 : 0.0f;

        auto d_omega_mech_dt = (torqueElec - parameters.B.Value() * motorState.omega_mech.Value() - loadOpposing - loads.externalTorque.Value() - cogging) / parameters.J.Value();

        float omegaMech = motorState.omega_mech.Value() + d_omega_mech_dt * dt;

        if (nonIdealities.coulombFrictionNm > 0.0f)
        {
            const float frictionStep = nonIdealities.coulombFrictionNm * dt / parameters.J.Value();
            omegaMech = std::fabs(omegaMech) <= frictionStep ? 0.0f : omegaMech - std::copysign(frictionStep, omegaMech);
        }

        motorState.omega_mech = foc::RadiansPerSecond{ omegaMech };
        motorState.omega = foc::RadiansPerSecond{ parameters.p * motorState.omega_mech.Value() };

        motorState.theta_mech += foc::Radians{ motorState.omega_mech.Value() * dt };
        motorState.theta += foc::Radians{ motorState.omega.Value() * dt };

        motorState.theta_mech = foc::Radians{ detail::PositionWithWrapAround(motorState.theta_mech.Value()) };
        motorState.theta = foc::Radians{ detail::PositionWithWrapAround(motorState.theta.Value()) };
    }
}
