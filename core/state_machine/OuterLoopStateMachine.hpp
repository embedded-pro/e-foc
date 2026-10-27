#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/services/cogging_compensation/CoggingMapping.hpp"
#include "core/services/cogging_compensation/CoggingMappingImpl.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/state_machine/FocStateMachineCommon.hpp"
#include "infra/util/WithSharedAccess.hpp"
#include <optional>

namespace application
{
    struct OuterLoopArgs
    {
        foc::Ampere maxCurrent;
        hal::Hertz baseFrequency;
        foc::LowPriorityInterrupt& lowPriorityInterrupt;
        hal::Hertz outerLoopFrequency{ 1000 };
        uint32_t dutyUpdateDelayPeriods{ 0 };
    };

    class OuterLoopStateMachine
        : public FocStateMachineCommon
    {
    protected:
        ~OuterLoopStateMachine() = default;

    public:
        state_machine::AppliedEstimates ApplyOnlineEstimates() override;
        std::optional<state_machine::OnlineEstimates> CurrentOnlineEstimates() override;
        void TraceOnlineEstimates();
        float AutomaticSpeedLoopBandwidth() const;

    protected:
        OuterLoopStateMachine(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            foc::Ampere driveCurrentLimit,
            NvmActivity& nvmActivity);

        bool HasModeSpecificWorkPending() const override;
        void ApplyModeSpecificCalibration(const services::CalibrationData& data) override;
        bool HasValidModeSpecificCalibration(const services::CalibrationData& data) const override;
        void PrepareForEnabled() override;
        void RegisterModeSpecificCli(services::TerminalWithStorage& terminal) override;
        void RunPostAlignmentStep(state_machine::Calibrating& calibrating) override;
        bool RunPostMechanicalStep(state_machine::Calibrating& calibrating) override;
        void AbortModeSpecificServices() override;

        void RunMechanicalIdentStep(state_machine::Calibrating& calibrating);

        static services::MechanicalParametersIdentification& ResolveMechIdent(
            const CalibrationServices& calibServices,
            std::optional<infra::WithSharedAccess<services::MechanicalParametersIdentificationImpl>>& ownMechIdent,
            foc::SpeedCommandable& speedCommandable,
            foc::Controllable& drive,
            foc::PhaseCurrentsObservable& observable,
            drivers::ThreePhaseInverter& inverter,
            drivers::Encoder& encoder);

        static services::CoggingMapping* ResolveCoggingMapping(
            const CalibrationServices& calibServices,
            std::optional<infra::WithSharedAccess<services::CoggingMappingImpl>>& ownCoggingMapping,
            foc::SpeedCommandable& speedCommandable,
            foc::Controllable& drive,
            foc::PhaseCurrentsObservable& observable,
            drivers::ThreePhaseInverter& inverter,
            drivers::Encoder& encoder);

        virtual foc::SpeedLoopTunable& SpeedTunable() = 0;
        virtual services::MechanicalParametersIdentification& MechIdentImpl() = 0;
        virtual const services::MechanicalParametersIdentification& MechIdentImpl() const = 0;
        virtual services::RealTimeFrictionAndInertiaEstimator& GetOnlineMechEstimator() = 0;
        virtual services::RealTimeResistanceAndInductanceEstimator& GetOnlineElecEstimator() = 0;
        virtual services::CoggingMapping* CoggingMappingImpl() = 0;
        virtual const services::CoggingMapping* CoggingMappingImpl() const = 0;
        virtual void LoadCoggingCompensation(infra::MemoryRange<const int8_t> torque, float torqueScale, foc::NewtonMeter torqueConstant) = 0;
        virtual void ClearCoggingCompensation() = 0;

    private:
        bool ApplyMechanics(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction, float bandwidth, foc::NewtonMeter torqueConstant);
        bool ApplyIdentificationControl(const services::CalibrationData& pending);
        foc::NewtonMeter TorqueConstantFor(const services::CalibrationData& data) const;
        services::MechanicalParametersIdentification::Config ExcitationConfig() const;
        services::CoggingMapping::Config CoggingConfig() const;
        void ApplyCoggingMap(const services::CalibrationData& data);

        static constexpr float velocityBandwidthRadPerSec = 50.0f;

        // Small placeholders that let the loops move the rotor during identification; never persisted.
        static constexpr float provisionalInertia = 1.0e-4f;
        static constexpr float provisionalFriction = 0.0f;
        static constexpr float identificationBandwidthRadPerSec = 20.0f;
        static constexpr float identificationSpeedMarginFactor = 2.0f;

        foc::Ampere driveCurrentLimit;
    };
}
