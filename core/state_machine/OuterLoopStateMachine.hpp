#pragma once

#include "core/foc/implementations/WithAutomaticSpeedPidGains.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/state_machine/FocStateMachineCommon.hpp"

namespace application
{
    struct OuterLoopArgs
    {
        foc::Ampere maxCurrent;
        hal::Hertz baseFrequency;
        foc::LowPriorityInterrupt& lowPriorityInterrupt;
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

        virtual foc::FocSpeedTunable& SpeedTunable() = 0;
        virtual foc::FocOnlineEstimableBase& OnlineEstimable() = 0;
        virtual services::MechanicalParametersIdentification& MechIdentImpl() = 0;
        virtual foc::WithAutomaticSpeedPidGains& GetSpeedAutoTuner() = 0;
        virtual services::RealTimeFrictionAndInertiaEstimator& GetOnlineMechEstimator() = 0;
        virtual services::RealTimeResistanceAndInductanceEstimator& GetOnlineElecEstimator() = 0;

    private:
        static constexpr float velocityBandwidthRadPerSec = 50.0f;

        foc::NewtonMeter mechTorqueConstant;
    };
}
