#pragma once

#include "core/foc/implementations/FocSpeedImpl.hpp"
#include "core/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "core/foc/implementations/WithAutomaticSpeedPidGains.hpp"
#include "core/foc/instantiations/FocController.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/state_machine/OuterLoopStateMachine.hpp"
#include <functional>
#include <optional>

namespace application
{
    class SpeedStateMachine
        : public OuterLoopStateMachine
    {
    public:
        SpeedStateMachine(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            state_machine::FaultNotifier& faultNotifier,
            state_machine::TransitionPolicy transitionPolicy,
            foc::Ampere maxCurrent,
            hal::Hertz baseFrequency,
            foc::LowPriorityInterrupt& lowPriorityInterrupt);

        foc::FocSpeed& GetController();

    protected:
        foc::FocBase& GetFoc() override;
        foc::Controllable& GetFocControl() override;
        foc::FocSpeedTunable& SpeedTunable() override;
        foc::FocOnlineEstimableBase& OnlineEstimable() override;
        services::MechanicalParametersIdentification& MechIdentImpl() override;
        foc::WithAutomaticCurrentPidGains& GetCurrentLoopTuner() override;
        foc::WithAutomaticSpeedPidGains& GetSpeedAutoTuner() override;
        services::RealTimeFrictionAndInertiaEstimator& GetOnlineMechEstimator() override;
        services::RealTimeResistanceAndInductanceEstimator& GetOnlineElecEstimator() override;

    private:
        static services::MechanicalParametersIdentification& ResolveMechIdent(
            const CalibrationServices& calibServices,
            std::optional<services::MechanicalParametersIdentificationImpl>& ownMechIdent,
            foc::FocSpeedController& focController,
            foc::ThreePhaseInverter& inverter,
            foc::Encoder& encoder);

        foc::FocSpeedController focController;
        foc::WithAutomaticCurrentPidGains pidAutoTuner;
        foc::WithAutomaticSpeedPidGains speedAutoTuner;
        services::RealTimeFrictionAndInertiaEstimator onlineMechEstimator;
        services::RealTimeResistanceAndInductanceEstimator onlineElecEstimator;
        std::optional<services::MechanicalParametersIdentificationImpl> ownMechIdent;
        std::reference_wrapper<services::MechanicalParametersIdentification> resolvedMechIdent;
    };
}
