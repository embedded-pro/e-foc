#pragma once

#ifndef Q_MOC_RUN
#include "core/foc/implementations/TransformsClarkePark.hpp"
#endif
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
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

        virtual void Started() = 0;
        virtual void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) = 0;
        virtual void Finished() = 0;
    };

    class ThreePhaseMotorModel
        : public foc::ThreePhaseInverter
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

        ThreePhaseMotorModel(const Parameters& params, foc::Volts powerSupplyVoltage, hal::Hertz baseFrequency, std::optional<std::size_t> maxIterations);

        void SetLoad(foc::NewtonMeter load);

        // Implementation of foc::ThreePhaseInverter
        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

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

        [[no_unique_address]] foc::Clarke clarke;
        [[no_unique_address]] foc::Park park;

        infra::Function<void(foc::PhaseCurrents)> onCurrentPhasesReady;
        bool running = false;
        std::optional<foc::NewtonMeter> load;
        const std::optional<std::size_t> maxIterations;
        std::optional<std::size_t> counter;
    };

    class SimulationFinishedObserver
        : public ThreePhaseMotorModelObserver
    {
    public:
        SimulationFinishedObserver(ThreePhaseMotorModel& model, const infra::Function<void()>& onFinished);

        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta) override;
        void Finished() override;

    private:
        infra::Function<void()> onFinished;
    };
}
