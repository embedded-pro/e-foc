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
#include <random>

namespace simulator
{
    class ThreePhaseMotorModel;

    class ThreePhaseMotorModelObserver
        : public infra::Observer<ThreePhaseMotorModelObserver, ThreePhaseMotorModel>
    {
    public:
        using infra::Observer<ThreePhaseMotorModelObserver, ThreePhaseMotorModel>::Observer;

        virtual void Started() = 0;
        virtual void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta, foc::RadiansPerSecond omegaMech) = 0;
        virtual void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) = 0;
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

        struct NoiseConfig
        {
            float sigmaAmpere{ 0.0f };
            float biasAmpereA{ 0.0f };
            float biasAmpereB{ 0.0f };
            float biasAmpereC{ 0.0f };
        };

        struct ThermalConfig
        {
            float ambientCelsius{ 25.0f };
            float thermalResistance{ 2.0f };   // °C/W
            float thermalCapacitance{ 25.0f }; // J/°C
            float copperTempCoeff{ 0.00393f }; // 1/°C
            float ironInductanceCoeff{ 0.0f }; // 1/°C
        };

        struct EncoderNoiseConfig
        {
            float sigmaRadians{ 0.0f };
            float biasRadians{ 0.0f };
        };

        ThreePhaseMotorModel(const Parameters& params, foc::Volts powerSupplyVoltage, hal::Hertz baseFrequency, std::optional<std::size_t> maxIterations);

        void SetLoad(foc::NewtonMeter load);
        void SetAdcNoise(const NoiseConfig& config);
        void SetEncoderNoise(const EncoderNoiseConfig& config);
        void SetThermalConfig(const ThermalConfig& config);
        void ResetTemperature();
        float WindingTemperatureCelsius() const;
        foc::Ohm EffectiveResistance() const;
        foc::Henry EffectiveInductanceD() const;
        foc::Henry EffectiveInductanceQ() const;
        void SetWindingTemperatureForTest(float celsius);

        // Enables continuous PWM cycling driven by the event dispatcher. Mirrors the
        // behavior of a real PWM peripheral whose carrier keeps running once enabled.
        // Once enabled, every call to ThreePhasePwmOutput becomes a duty-cycle register
        // update; the model schedules its own next sample via the dispatcher. Tests that
        // drive the model synchronously must NOT enable self-driving (use StepForTest).
        void EnableSelfDriving();

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

        // Synchronous single-cycle execution for unit tests. Bypasses the event
        // dispatcher and the controller callback so legacy step-driven tests can
        // observe the model state directly.
        void StepForTest(const foc::PhasePwmDutyCycles& dutyPhases);

    private:
        void Model(const foc::PhasePwmDutyCycles& dutyPhases);
        void RunOneCycle(const foc::PhasePwmDutyCycles& dutyPhases);
        void ScheduleNextCycle();
        float SampleNoise();

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

        NoiseConfig noiseConfig{};
        std::mt19937 noiseEngine{ 0xc0ffeeU };
        std::normal_distribution<float> noiseDistribution{ 0.0f, 1.0f };
        foc::Ampere iaNoisyLast{ 0.0f };
        foc::Ampere ibNoisyLast{ 0.0f };
        foc::Ampere icNoisyLast{ 0.0f };

        ThermalConfig thermalConfig{};
        float windingTempCelsius{ 25.0f };

        EncoderNoiseConfig encoderNoiseConfig{};
        std::mt19937 encoderNoiseEngine{ 0xbaadf00dU };
        std::normal_distribution<float> encoderNoiseDistribution{ 0.0f, 1.0f };

        bool selfDriving{ false };
        bool cycleScheduled{ false };
        foc::PhasePwmDutyCycles pendingDuties{ hal::Percent{ 50 }, hal::Percent{ 50 }, hal::Percent{ 50 } };
    };

    class SimulationFinishedObserver
        : public ThreePhaseMotorModelObserver
    {
    public:
        SimulationFinishedObserver(ThreePhaseMotorModel& model, const infra::Function<void()>& onFinished);

        void Started() override;
        void PhaseCurrentsWithMechanicalAngle(foc::PhaseCurrents currentPhases, foc::Radians theta, foc::RadiansPerSecond omegaMech) override;
        void StatorVoltages(foc::ThreePhase phaseVoltages, foc::TwoPhase alphaBeta) override;
        void Finished() override;

    private:
        infra::Function<void()> onFinished;
    };
}
