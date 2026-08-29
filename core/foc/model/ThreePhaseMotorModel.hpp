#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstddef>
#include <optional>
#include <random>

namespace foc
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
        : public drivers::ThreePhaseInverter
        , public drivers::Encoder
        , public infra::Subject<ThreePhaseMotorModelObserver>
    {
    public:
        struct Parameters
        {
            foc::Ohm R;
            foc::Henry Ld;
            foc::Henry Lq;
            foc::Weber psi_f;
            uint8_t p;

            foc::KilogramMeterSquared J;
            foc::NewtonMeterSecondPerRadian B;
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
            float thermalResistance{ 2.0f };
            float thermalCapacitance{ 25.0f };
            float copperTempCoeff{ 0.00393f };
            float ironInductanceCoeff{ 0.0f };
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

        void EnableSelfDriving();

        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;

        foc::Radians Read() override;
        void Set(foc::Radians value) override;
        void SetZero() override;

        void StepForTest(const foc::PhasePwmDutyCycles& dutyPhases);

    private:
        void Model(const foc::PhasePwmDutyCycles& dutyPhases);
        void RunOneCycle(const foc::PhasePwmDutyCycles& dutyPhases);
        void ScheduleNextCycle();
        float SampleNoise();

    private:
        struct MotorState
        {
            foc::Ampere ia{ 0.0f };
            foc::Ampere ib{ 0.0f };
            foc::Ampere ic{ 0.0f };
            foc::Radians theta{ 0.0f };
            foc::Radians theta_mech{ 0.0f };
            foc::RadiansPerSecond omega{ 0.0f };
            foc::RadiansPerSecond omega_mech{ 0.0f };
        };

        struct CurrentNoiseState
        {
            NoiseConfig config{};
            std::mt19937 engine{ std::random_device{}() };
            std::normal_distribution<float> distribution{ 0.0f, 1.0f };
            foc::Ampere iaLast{ 0.0f };
            foc::Ampere ibLast{ 0.0f };
            foc::Ampere icLast{ 0.0f };
        };

        struct ThermalState
        {
            ThermalConfig config{};
            float windingTempCelsius{ 25.0f };
        };

        struct EncoderNoiseState
        {
            EncoderNoiseConfig config{};
            std::mt19937 engine{ std::random_device{}() };
            std::normal_distribution<float> distribution{ 0.0f, 1.0f };
        };

        struct SelfDriveState
        {
            bool driving{ false };
            bool cycleScheduled{ false };
            foc::PhasePwmDutyCycles pendingDuties{ hal::Percent{ 50 }, hal::Percent{ 50 }, hal::Percent{ 50 } };
        };

        Parameters parameters;
        hal::Hertz baseFrequency;
        foc::Volts powerSupplyVoltage;
        MotorState motorState;

        [[no_unique_address]] foc::Clarke clarke;
        [[no_unique_address]] foc::Park park;

        infra::Function<void(foc::PhaseCurrents)> onCurrentPhasesReady;
        bool running{ false };
        std::optional<foc::NewtonMeter> load;
        const std::optional<std::size_t> maxIterations;
        std::optional<std::size_t> counter;

        CurrentNoiseState currentNoise;
        ThermalState thermal;
        EncoderNoiseState encoderNoise;
        SelfDriveState selfDrive;
    };
}
