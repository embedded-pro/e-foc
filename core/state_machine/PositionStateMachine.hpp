#pragma once

#include "core/foc/instantiations/FocController.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/state_machine/OuterLoopStateMachine.hpp"
#include <functional>
#include <optional>

namespace application
{
    class PositionStateMachine
        : public OuterLoopStateMachine
    {
    public:
        PositionStateMachine(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            state_machine::FaultNotifier& faultNotifier,
            state_machine::TransitionPolicy transitionPolicy,
            const OuterLoopArgs& outerLoopArgs);

        foc::FocPosition& GetController();

    protected:
        foc::FocBase& GetFoc() override;
        foc::Controllable& GetFocControl() override;
        foc::SpeedLoopTunable& SpeedTunable() override;
        foc::FocOnlineEstimableBase& OnlineEstimable() override;
        services::MechanicalParametersIdentification& MechIdentImpl() override;
        void RunPostAlignmentStep() override;
        foc::CurrentLoopTunable& CurrentTunable() override;
        services::RealTimeFrictionAndInertiaEstimator& GetOnlineMechEstimator() override;
        services::RealTimeResistanceAndInductanceEstimator& GetOnlineElecEstimator() override;

    private:
        foc::FocPositionController focController;
        services::RealTimeFrictionAndInertiaEstimator onlineMechEstimator;
        services::RealTimeResistanceAndInductanceEstimator onlineElecEstimator;
        std::optional<std::reference_wrapper<services::MechanicalParametersIdentification>> mechIdentPtr;
    };
}
