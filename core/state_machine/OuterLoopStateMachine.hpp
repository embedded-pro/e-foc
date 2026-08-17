#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/state_machine/FocStateMachineCommon.hpp"
#include <optional>

namespace application
{
    struct OuterLoopArgs
    {
        foc::Ampere maxCurrent;
        hal::Hertz baseFrequency;
        foc::LowPriorityInterrupt& lowPriorityInterrupt;
        hal::Hertz outerLoopFrequency{ 1000 };
    };

    class OuterLoopStateMachine
        : public FocStateMachineCommon
    {
    public:
        void ApplyOnlineEstimates() override;

    protected:
        OuterLoopStateMachine(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            services::ElectricalParametersIdentification& electricalIdent,
            services::MotorAlignment& motorAlignment,
            foc::NewtonMeter mechTorqueConstant);

        void ApplyModeSpecificCalibration(const services::CalibrationData& data) override;
        void PrepareForEnabled() override;
        void RegisterModeSpecificCli(services::TerminalWithStorage& terminal) override;
        void RunPostAlignmentStep() override;

        void RunMechanicalIdentStep();

        // Without an injected service the mode owns one, so a blank NVM can still be commissioned.
        static services::MechanicalParametersIdentification& ResolveMechIdent(
            const CalibrationServices& calibServices,
            std::optional<services::MechanicalParametersIdentificationImpl>& ownMechIdent,
            foc::SpeedCommandable& speedCommandable,
            drivers::ThreePhaseInverter& inverter,
            drivers::Encoder& encoder);

        virtual foc::SpeedLoopTunable& SpeedTunable() = 0;
        virtual services::MechanicalParametersIdentification& MechIdentImpl() = 0;
        virtual services::RealTimeFrictionAndInertiaEstimator& GetOnlineMechEstimator() = 0;
        virtual services::RealTimeResistanceAndInductanceEstimator& GetOnlineElecEstimator() = 0;

    private:
        void ApplyMechanics(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction, float bandwidth);

        static constexpr float velocityBandwidthRadPerSec = 50.0f;

        foc::NewtonMeter mechTorqueConstant;
    };
}
