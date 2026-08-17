#pragma once

#include "core/foc/cascade/SpeedCascade.hpp"
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
            const OuterLoopArgs& outerLoopArgs);

        foc::FocSpeed& GetController();

    protected:
        foc::FocBase& GetFoc() override;
        foc::Controllable& GetFocControl() override;
        foc::SpeedLoopTunable& SpeedTunable() override;
        services::MechanicalParametersIdentification& MechIdentImpl() override;
        foc::CurrentLoopTunable& CurrentTunable() override;
        services::RealTimeFrictionAndInertiaEstimator& GetOnlineMechEstimator() override;
        services::RealTimeResistanceAndInductanceEstimator& GetOnlineElecEstimator() override;

    private:
        foc::FocSpeedController focController;
        services::RealTimeFrictionAndInertiaEstimator onlineMechEstimator;
        services::RealTimeResistanceAndInductanceEstimator onlineElecEstimator;
        std::optional<services::MechanicalParametersIdentificationImpl> ownMechIdent;
        std::reference_wrapper<services::MechanicalParametersIdentification> resolvedMechIdent;
    };
}
