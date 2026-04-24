#include "tools/simulator/model/Model.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "foc/implementations/TrigonometricImpl.hpp"
#include "foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include <cmath>
#include <numbers>

namespace simulator
{
    namespace
    {
        constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;
        constexpr float half = 0.5f;
        constexpr float percentToFraction = 100.0f;
        constexpr float torqueConstant = 1.5f;

        float WrapAngle(float angle)
        {
            auto result = std::fmod(angle, two_pi);
            if (result < 0.0f)
                result += two_pi;
            return result;
        }
    }

    ThreePhaseMotorModel::ThreePhaseMotorModel(const Parameters& params, foc::Volts powerSupplyVoltage, hal::Hertz baseFrequency, std::optional<std::size_t> maxIterations)
        : parameters(params)
        , baseFrequency(baseFrequency)
        , powerSupplyVoltage(powerSupplyVoltage)
        , maxIterations(maxIterations)
    {
    }

    void ThreePhaseMotorModel::SetLoad(foc::NewtonMeter load)
    {
        this->load.emplace(load);
    }

    void ThreePhaseMotorModel::SetAdcNoise(const NoiseConfig& config)
    {
        noiseConfig = config;
    }

    void ThreePhaseMotorModel::SetEncoderNoise(const EncoderNoiseConfig& config)
    {
        encoderNoiseConfig = config;
    }

    void ThreePhaseMotorModel::SetThermalConfig(const ThermalConfig& config)
    {
        thermalConfig = config;
    }

    void ThreePhaseMotorModel::ResetTemperature()
    {
        windingTempCelsius = thermalConfig.ambientCelsius;
    }

    float ThreePhaseMotorModel::WindingTemperatureCelsius() const
    {
        return windingTempCelsius;
    }

    foc::Ohm ThreePhaseMotorModel::EffectiveResistance() const
    {
        const float deltaT = windingTempCelsius - thermalConfig.ambientCelsius;
        return foc::Ohm{ parameters.R.Value() * (1.0f + thermalConfig.copperTempCoeff * deltaT) };
    }

    foc::Henry ThreePhaseMotorModel::EffectiveInductanceD() const
    {
        const float deltaT = windingTempCelsius - thermalConfig.ambientCelsius;
        return foc::Henry{ parameters.Ld.Value() * (1.0f - thermalConfig.ironInductanceCoeff * deltaT) };
    }

    foc::Henry ThreePhaseMotorModel::EffectiveInductanceQ() const
    {
        const float deltaT = windingTempCelsius - thermalConfig.ambientCelsius;
        return foc::Henry{ parameters.Lq.Value() * (1.0f - thermalConfig.ironInductanceCoeff * deltaT) };
    }

    void ThreePhaseMotorModel::SetWindingTemperatureForTest(float celsius)
    {
        windingTempCelsius = celsius;
    }

    float ThreePhaseMotorModel::SampleNoise()
    {
        return noiseConfig.sigmaAmpere * noiseDistribution(noiseEngine);
    }

    void ThreePhaseMotorModel::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        this->baseFrequency = baseFrequency;
        onCurrentPhasesReady = onDone;
    }

    void ThreePhaseMotorModel::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        // Mirror real PWM hardware: writing to the duty registers latches the new
        // duties and (re-)arms the carrier. The carrier keeps cycling at the base
        // frequency until Stop() is called. Re-entrant calls from within the
        // controller callback simply update the latched duties.
        pendingDuties = dutyPhases;
        EnableSelfDriving();
    }

    void ThreePhaseMotorModel::StepForTest(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        pendingDuties = dutyPhases;
        RunOneCycle(dutyPhases);
    }

    void ThreePhaseMotorModel::RunOneCycle(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        Model(dutyPhases);

        bool justFinished = false;
        if (counter.has_value() && --counter.value() == 0)
        {
            running = false;
            justFinished = true;
        }

        const auto iaNoise = foc::Ampere{ ia.Value() + noiseConfig.biasAmpereA + SampleNoise() };
        const auto ibNoise = foc::Ampere{ ib.Value() + noiseConfig.biasAmpereB + SampleNoise() };
        const auto icNoise = foc::Ampere{ ic.Value() + noiseConfig.biasAmpereC + SampleNoise() };
        iaNoisyLast = iaNoise;
        ibNoisyLast = ibNoise;
        icNoisyLast = icNoise;

        const auto va = (dutyPhases.a.Value() / percentToFraction - half) * powerSupplyVoltage.Value();
        const auto vb = (dutyPhases.b.Value() / percentToFraction - half) * powerSupplyVoltage.Value();
        const auto vc = (dutyPhases.c.Value() / percentToFraction - half) * powerSupplyVoltage.Value();
        const foc::ThreePhase vAbc{ va, vb, vc };
        const auto vAlphaBeta = clarke.Forward(vAbc);

        NotifyObservers([this, &vAbc, &vAlphaBeta](auto& observer)
            {
                observer.PhaseCurrentsWithMechanicalAngle({ iaNoisyLast, ibNoisyLast, icNoisyLast }, theta_mech, omega_mech);
                observer.StatorVoltages(vAbc, vAlphaBeta);
            });

        // Invoke the controller callback directly. Any re-entrant call to
        // ThreePhasePwmOutput from inside the callback only updates pendingDuties,
        // so the dispatcher queue never grows.
        if (onCurrentPhasesReady && !justFinished)
            onCurrentPhasesReady({ iaNoisyLast, ibNoisyLast, icNoisyLast });

        if (justFinished)
        {
            selfDriving = false;
            NotifyObservers([](auto& observer)
                {
                    observer.Finished();
                });
        }
    }

    void ThreePhaseMotorModel::EnableSelfDriving()
    {
        if (selfDriving)
            return;

        selfDriving = true;
        ScheduleNextCycle();
    }

    void ThreePhaseMotorModel::ScheduleNextCycle()
    {
        if (cycleScheduled || !selfDriving)
            return;

        cycleScheduled = true;
        infra::EventDispatcherWithWeakPtr::Instance().Schedule([this]()
            {
                cycleScheduled = false;
                if (!selfDriving)
                    return;
                RunOneCycle(pendingDuties);
                ScheduleNextCycle();
            });
    }

    void ThreePhaseMotorModel::Start()
    {
        if (running)
            return;

        running = true;
        counter = maxIterations;
        ia = foc::Ampere{ 0.0f };
        ib = foc::Ampere{ 0.0f };
        ic = foc::Ampere{ 0.0f };
        theta = foc::Radians{ 0.0f };
        theta_mech = foc::Radians{ 0.0f };
        omega = foc::RadiansPerSecond{ 0.0f };
        omega_mech = foc::RadiansPerSecond{ 0.0f };
        ResetTemperature();
        pendingDuties = foc::PhasePwmDutyCycles{ hal::Percent{ 50 }, hal::Percent{ 49 }, hal::Percent{ 51 } };

        NotifyObservers([](auto& observer)
            {
                observer.Started();
            });

        // Real PWM peripheral starts cycling on enable; the model mirrors this by
        // self-driving until Stop().
        EnableSelfDriving();
    }

    void ThreePhaseMotorModel::Stop()
    {
        running = false;
        selfDriving = false;
        counter = std::nullopt;
        ia = foc::Ampere{ 0.0f };
        ib = foc::Ampere{ 0.0f };
        ic = foc::Ampere{ 0.0f };
        pendingDuties = foc::PhasePwmDutyCycles{ hal::Percent{ 50 }, hal::Percent{ 50 }, hal::Percent{ 50 } };
        // Drop the callback so a stale controller cannot be invoked when a calibration
        // service writes new duties before installing its own callback.
        onCurrentPhasesReady = nullptr;
    }

    hal::Hertz ThreePhaseMotorModel::BaseFrequency() const
    {
        return baseFrequency;
    }

    foc::Ampere ThreePhaseMotorModel::MaxCurrentSupported() const
    {
        return foc::Ampere{ 20.0f };
    }

    foc::Radians ThreePhaseMotorModel::Read()
    {
        const float noise = encoderNoiseConfig.sigmaRadians * encoderNoiseDistribution(encoderNoiseEngine);
        return foc::Radians{ WrapAngle(theta_mech.Value() + encoderNoiseConfig.biasRadians + noise) };
    }

    void ThreePhaseMotorModel::Set(foc::Radians value)
    {
        theta_mech = value;
        theta = foc::Radians{ parameters.p * theta_mech.Value() };
    }

    void ThreePhaseMotorModel::SetZero()
    {
        theta_mech = foc::Radians{ 0.0f };
        theta = foc::Radians{ 0.0f };
    }

    void ThreePhaseMotorModel::Model(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        auto dt = 1.0f / static_cast<float>(baseFrequency.Value());
        auto duty_a = dutyPhases.a.Value() / percentToFraction;
        auto duty_b = dutyPhases.b.Value() / percentToFraction;
        auto duty_c = dutyPhases.c.Value() / percentToFraction;

        auto va = (duty_a - half) * powerSupplyVoltage;
        auto vb = (duty_b - half) * powerSupplyVoltage;
        auto vc = (duty_c - half) * powerSupplyVoltage;

        auto cos_theta = foc::FastTrigonometry::Cosine(theta.Value());
        auto sin_theta = foc::FastTrigonometry::Sine(theta.Value());

        auto v_dq = park.Forward(clarke.Forward(foc::ThreePhase{ va.Value(), vb.Value(), vc.Value() }), cos_theta, sin_theta);
        auto i_dq = park.Forward(clarke.Forward(foc::ThreePhase{ ia.Value(), ib.Value(), ic.Value() }), cos_theta, sin_theta);

        auto id = i_dq.d;
        auto iq = i_dq.q;

        const auto rEff = EffectiveResistance().Value();
        const auto ldEff = EffectiveInductanceD().Value();
        const auto lqEff = EffectiveInductanceQ().Value();

        auto dId_dt = (v_dq.d - rEff * id + omega.Value() * lqEff * iq) / ldEff;
        auto dIq_dt = (v_dq.q - rEff * iq - omega.Value() * ldEff * id - omega.Value() * parameters.psi_f.Value()) / lqEff;

        id += dId_dt * dt;
        iq += dIq_dt * dt;

        auto i_abc = clarke.Inverse(park.Inverse(foc::RotatingFrame{ id, iq }, cos_theta, sin_theta));

        ia = foc::Ampere{ i_abc.a };
        ib = foc::Ampere{ i_abc.b };
        ic = foc::Ampere{ i_abc.c };

        // Thermal update
        const auto pCu = rEff * (ia.Value() * ia.Value() + ib.Value() * ib.Value() + ic.Value() * ic.Value());
        windingTempCelsius += dt * (pCu - (windingTempCelsius - thermalConfig.ambientCelsius) / thermalConfig.thermalResistance) / thermalConfig.thermalCapacitance;

        auto torqueElec = torqueConstant * parameters.p * (parameters.psi_f.Value() * iq + (ldEff - lqEff) * id * iq);

        auto loadTorque = load.value_or(foc::NewtonMeter{ 0.0f }).Value();
        auto loadOpposing = (omega_mech.Value() >= 0.0f) ? loadTorque : -loadTorque;

        auto d_omega_mech_dt = (torqueElec - parameters.B.Value() * omega_mech.Value() - loadOpposing) / parameters.J.Value();

        omega_mech += foc::RadiansPerSecond{ d_omega_mech_dt * dt };
        omega = foc::RadiansPerSecond{ parameters.p * omega_mech.Value() };

        theta_mech += foc::Radians{ omega_mech.Value() * dt };
        theta += foc::Radians{ omega.Value() * dt };

        theta_mech = foc::Radians{ WrapAngle(theta_mech.Value()) };
        theta = foc::Radians{ WrapAngle(theta.Value()) };
    }

    SimulationFinishedObserver::SimulationFinishedObserver(ThreePhaseMotorModel& model, const infra::Function<void()>& onFinished)
        : ThreePhaseMotorModelObserver(model)
        , onFinished(onFinished)
    {
    }

    void SimulationFinishedObserver::Started()
    {
    }

    void SimulationFinishedObserver::PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents /*currentPhases*/, foc::Radians /*theta*/, foc::RadiansPerSecond /*omegaMech*/)
    {
    }

    void SimulationFinishedObserver::StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta)
    {
    }

    void SimulationFinishedObserver::Finished()
    {
        onFinished();
    }
}
