#pragma once

#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/foc/model/CurrentSense.hpp"
#include "core/foc/model/ElectricalPlant.hpp"
#include "core/foc/model/EncoderModel.hpp"
#include "core/foc/model/MechanicalPlant.hpp"
#include "core/foc/model/PhaseCurrentSensor.hpp"
#include "core/foc/model/PowerStage.hpp"
#include "core/foc/model/SimulatedEncoder.hpp"
#include "core/foc/model/SupplyModel.hpp"
#include "core/foc/model/ThermalModel.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

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
    };

    class ThreePhaseMotorModel
        : public drivers::ThreePhaseInverter
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

        ThreePhaseMotorModel(const Parameters& params, foc::Volts powerSupplyVoltage, hal::Hertz baseFrequency);

        MechanicalPlant& Mechanics();
        ElectricalPlant& Electrical();
        ThermalModel& Thermal();
        SupplyModel& Supply();
        PhaseCurrentSensor& CurrentSensor();
        SimulatedEncoder& Encoder();
        WindingParameters Winding() const;

        void PhaseCurrentsReady(hal::Hertz baseFrequency, const infra::Function<void(foc::PhaseCurrents)>& onDone) override;
        void ThreePhasePwmOutput(const foc::PhasePwmDutyCycles& dutyPhases) override;
        void Start() override;
        void Stop() override;
        hal::Hertz BaseFrequency() const override;
        foc::Ampere MaxCurrentSupported() const override;
        void CurrentOffsets(const foc::PhaseCurrents& offsets) override;
        void ZeroVectorSlopes(const foc::PhaseCurrentSlopes& slopes) override;

        void StepForTest(const foc::PhasePwmDutyCycles& dutyPhases);

    private:
        void RunOneCycle(const foc::PhasePwmDutyCycles& dutyPhases);

        Parameters parameters;
        hal::Hertz baseFrequency;
        float period;

        [[no_unique_address]] foc::Clarke clarke;

        infra::Function<void(foc::PhaseCurrents)> onCurrentPhasesReady;
        bool running{ false };

        ElectricalPlant electrical;
        MechanicalPlant mechanical;
        ThermalModel thermal;
        SupplyModel supply;
        SimulatedEncoder encoder{ mechanical };
        PhaseCurrentSensor currentSensor;
        foc::ThreePhase lastPhaseVoltages{};
    };

    void InjectFaults(ThreePhaseMotorModel& model, const ThreePhaseMotorModel::FaultInjectionConfig& config);
    void SeedNoise(ThreePhaseMotorModel& model, uint32_t seed);
}
