#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/model/CurrentSense.hpp"
#include "core/foc/model/ElectricalPlant.hpp"
#include "core/foc/model/EncoderModel.hpp"
#include "core/foc/model/MechanicalPlant.hpp"
#include "core/foc/model/PhaseCurrentSensor.hpp"
#include "core/foc/model/PowerStage.hpp"
#include "core/foc/model/SupplyModel.hpp"
#include "core/foc/model/ThermalModel.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "infra/util/SharedPtr.hpp"
#include <cstddef>
#include <optional>

namespace foc
{
    class ThreePhaseMotorModel;

    class ThreePhaseMotorModelObserver
        : public infra::Observer<ThreePhaseMotorModelObserver, ThreePhaseMotorModel>
    {
    protected:
        ~ThreePhaseMotorModelObserver() = default;

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
        , public infra::EnableSharedFromThis<ThreePhaseMotorModel>
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

            foc::Ampere maxSupportedCurrent{ 20.0f };
        };

        using NoiseConfig = CurrentNoiseConfig;
        using ThermalConfig = foc::ThermalConfig;
        using EncoderNoiseConfig = foc::EncoderNoiseConfig;
        using SupplyConfig = foc::SupplyConfig;

        struct FaultInjectionConfig
        {
            bool openPhaseA{ false };
            bool openPhaseB{ false };
            bool openPhaseC{ false };
            bool encoderStuck{ false };
            float supplyVoltageScale{ 1.0f };
        };

        ThreePhaseMotorModel(const Parameters& params, foc::Volts powerSupplyVoltage, hal::Hertz baseFrequency, std::optional<std::size_t> maxIterations, bool selfDriveEnabled = true);

        void SetLoad(foc::NewtonMeter load);
        void SetExternalTorque(foc::NewtonMeter torque);
        foc::NewtonMeter ExternalTorque() const;
        void GripShaft(std::optional<foc::RadiansPerSecond> handSpeed);
        foc::RadiansPerSecond MechanicalSpeed() const;
        foc::Radians MechanicalAngle() const;
        foc::RotatingFrame LastDqCurrents() const;
        void SetAdcNoise(const NoiseConfig& config);
        void SetEncoderNoise(const EncoderNoiseConfig& config);
        void SetThermalConfig(const ThermalConfig& config);
        void SetFaultInjection(const FaultInjectionConfig& config);
        void SetSupply(const SupplyConfig& config);
        void SetPowerStage(const PowerStageConfig& config);
        void SetCurrentSense(const CurrentSenseConfig& config);
        void SetCoulombFriction(foc::NewtonMeter friction);
        void SetCogging(foc::NewtonMeter amplitude, uint32_t cyclesPerRevolution);
        void SetEncoderResolution(uint32_t countsPerRevolution);
        void SetRandomSeed(uint32_t seed);
        void ResetTemperature();
        float WindingTemperatureCelsius() const;
        foc::PhaseCurrents LastMeasuredCurrents() const;
        foc::Volts EffectiveSupplyVoltage() const;
        foc::Ampere SupplyCurrent() const;
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
        float Period() const;
        WindingParameters Winding() const;

    private:
        struct SelfDriveState
        {
            bool driving{ false };
            bool cycleScheduled{ false };
            foc::PhasePwmDutyCycles pendingDuties{ hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(50) };
        };

        Parameters parameters;
        hal::Hertz baseFrequency;

        [[no_unique_address]] foc::Clarke clarke;

        infra::Function<void(foc::PhaseCurrents)> onCurrentPhasesReady;
        bool running{ false };
        const bool selfDriveEnabled;
        const std::optional<std::size_t> maxIterations;
        std::optional<std::size_t> counter;

        ElectricalPlant electrical;
        MechanicalPlant mechanical;
        ThermalModel thermal;
        SupplyModel supply;
        EncoderModel encoder;
        PhaseCurrentSensor currentSensor;
        foc::ThreePhase lastPhaseVoltages{};
        SelfDriveState selfDrive;
    };
}
