#include "simulator/model/Model.hpp"
#include "foc/instantiations/TrigonometricImpl.hpp"
#include "foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include <cmath>

namespace simulator
{
    namespace
    {
        constexpr float two_pi = 6.28318530718f;
        constexpr float half = 0.5f;
    }

    ThreePhaseMotorModel::ThreePhaseMotorModel(const Parameters& params)
        : parameters(params)
    {
    }

    void ThreePhaseMotorModel::SetLoad(foc::NewtonMeter load)
    {
        this->load.emplace(load);
    }

    void ThreePhaseMotorModel::PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents currentPhases)>& onDone)
    {
        this->baseFrequency = baseFrequency;
        onCurrentPhasesReady = onDone;
    }

    void ThreePhaseMotorModel::ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases)
    {
        auto dt = 1.0f / baseFrequency.Value();
        auto duty_a = dutyPhases.a.Value() / 100.0f;
        auto duty_b = dutyPhases.b.Value() / 100.0f;
        auto duty_c = dutyPhases.c.Value() / 100.0f;

        auto va = (duty_a - half) * parameters.Vdc;
        auto vb = (duty_b - half) * parameters.Vdc;
        auto vc = (duty_c - half) * parameters.Vdc;

        auto cos_theta = foc::FastTrigonometry::Cosine(theta.Value());
        auto sin_theta = foc::FastTrigonometry::Sine(theta.Value());

        auto v_dq = park.Forward(clarke.Forward(foc::ThreePhase{ va.Value(), vb.Value(), vc.Value() }), cos_theta, sin_theta);
        auto i_dq = park.Forward(clarke.Forward(foc::ThreePhase{ ia.Value(), ib.Value(), ic.Value() }), cos_theta, sin_theta);

        auto id = i_dq.d;
        auto iq = i_dq.q;

        auto dId_dt = (v_dq.d - parameters.R.Value() * id + omega.Value() * parameters.Lq.Value() * iq) / parameters.Ld.Value();
        auto dIq_dt = (v_dq.q - parameters.R.Value() * iq - omega.Value() * parameters.Ld.Value() * id - omega.Value() * parameters.psi_f.Value()) / parameters.Lq.Value();

        id += dId_dt * dt;
        iq += dIq_dt * dt;

        auto i_abc = clarke.Inverse(park.Inverse(foc::RotatingFrame{ id, iq }, cos_theta, sin_theta));

        ia = foc::Ampere{ i_abc.a };
        ib = foc::Ampere{ i_abc.b };
        ic = foc::Ampere{ i_abc.c };

        auto torqueElec = 1.5f * parameters.p * (parameters.psi_f.Value() * iq + (parameters.Ld.Value() - parameters.Lq.Value()) * id * iq);
        auto d_omega_mech_dt = (torqueElec - parameters.B.Value() * omega_mech.Value() - (load.value_or(foc::NewtonMeter{ 0.0f }).Value())) / parameters.J.Value();

        omega_mech += foc::RadiansPerSecond{ d_omega_mech_dt * dt };
        omega = foc::RadiansPerSecond{ parameters.p * omega_mech.Value() };

        theta_mech += foc::Radians{ omega_mech.Value() * dt };
        theta += foc::Radians{ omega.Value() * dt };

        theta_mech = foc::Radians{ std::fmod(theta_mech.Value(), two_pi) };
        theta = foc::Radians{ std::fmod(theta.Value(), two_pi) };

        if (onCurrentPhasesReady && running)
            onCurrentPhasesReady({ foc::Ampere{ ia }, foc::Ampere{ ib }, foc::Ampere{ ic } });

        if (running)
            NotifyObservers([&](auto& observer)
                {
                    observer.PhaseCurrentsWithMechanicalAngle({ ia, ib, ic }, theta_mech);
                });
    }

    void ThreePhaseMotorModel::Start()
    {
        running = true;
        ia = foc::Ampere{ 0.0f };
        ib = foc::Ampere{ 0.0f };
        ic = foc::Ampere{ 0.0f };
    }

    void ThreePhaseMotorModel::Stop()
    {
        running = false;
        ia = foc::Ampere{ 0.0f };
        ib = foc::Ampere{ 0.0f };
        ic = foc::Ampere{ 0.0f };
    }

    hal::Hertz ThreePhaseMotorModel::BaseFrequency() const
    {
        return baseFrequency;
    }

    foc::Radians ThreePhaseMotorModel::Read()
    {
        return theta_mech;
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
}
