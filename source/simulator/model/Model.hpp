
#pragma once

#include "infra/util/Observer.hpp"
#include "source/foc/implementations/TransformsClarkePark.hpp"
#include "source/foc/interfaces/Driver.hpp"
#include "source/foc/interfaces/Units.hpp"
#include <cstddef>
#include <optional>

namespace simulator
{
    class ThreePhaseMotorModel;

    class ThreePhaseMotorModelObserver
        : public infra::Observer<ThreePhaseMotorModelObserver, ThreePhaseMotorModel>
    {
    public:
        using infra::Observer<ThreePhaseMotorModelObserver, ThreePhaseMotorModel>::Observer;

        virtual void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) = 0;
        virtual void Finished() = 0;
    };

    class ThreePhaseMotorModel
        : public foc::MotorDriver
        , public foc::Encoder
        , public infra::Subject<ThreePhaseMotorModelObserver>
    {
    public:
        struct Parameters
        {
            // Motor electrical parameters (based on typical small PMSM, similar to Maxon EC 45)
            foc::Ohm R;       // Phase resistance [Ohm]
            foc::Henry Ld;    // d-axis inductance [H]
            foc::Henry Lq;    // q-axis inductance [H] (SPM: Ld ≈ Lq)
            foc::Weber psi_f; // Permanent magnet flux linkage [Wb]
            uint8_t p;        // Pole pairs

            // Mechanical parameters
            foc::KilogramMeterSquared J;       // Rotor inertia [kg·m²]
            foc::NewtonMeterSecondPerRadian B; // Viscous friction coefficient [N·m·s/rad]
        };

        ThreePhaseMotorModel(const Parameters& params, foc::Volts powerSupplyVoltage, std::optional<std::size_t> maxIterations);

        void SetLoad(foc::NewtonMeter load);

        // Implementation of foc::MotorDriver
        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;

        // Implementation of foc::Encoder
        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

    private:
        void Model(const foc::PhasePwmDutyCycles& dutyPhases);

    private:
        Parameters parameters;
        hal::Hertz baseFrequency;
        foc::Volts powerSupplyVoltage;

        foc::Ampere ia{ 0.0f };
        foc::Ampere ib{ 0.0f };
        foc::Ampere ic{ 0.0f };
        foc::Radians theta{ 0.0f };
        foc::Radians theta_mech{ 0.0f };
        foc::RadiansPerSecond omega{ 0.0f };
        foc::RadiansPerSecond omega_mech{ 0.0f };

        foc::Clarke clarke;
        foc::Park park;

        infra::Function<void(foc::PhaseCurrents)> onCurrentPhasesReady;
        bool running = false;
        std::optional<foc::NewtonMeter> load;
        const std::optional<std::size_t> maxIterations;
        std::optional<std::size_t> counter;
    };
}
