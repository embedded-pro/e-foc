#pragma once

#include "core/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "core/foc/implementations/WithAutomaticSpeedPidGains.hpp"
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
            foc::Ampere maxCurrent,
            hal::Hertz baseFrequency,
            foc::LowPriorityInterrupt& lowPriorityInterrupt);

        foc::FocPosition& GetController();

    protected:
        foc::FocBase& GetFoc() override;
        foc::Controllable& GetFocControl() override;
        foc::FocSpeedTunable& SpeedTunable() override;
        foc::FocOnlineEstimableBase& OnlineEstimable() override;
        services::MechanicalParametersIdentification& MechIdentImpl() override;
        void RunPostAlignmentStep() override;
        foc::WithAutomaticCurrentPidGains& GetCurrentLoopTuner() override;
        foc::WithAutomaticSpeedPidGains& GetSpeedAutoTuner() override;
        services::RealTimeFrictionAndInertiaEstimator& GetOnlineMechEstimator() override;
        services::RealTimeResistanceAndInductanceEstimator& GetOnlineElecEstimator() override;

    private:
        foc::FocPositionController focController;
        foc::WithAutomaticCurrentPidGains pidAutoTuner;
        foc::WithAutomaticSpeedPidGains speedAutoTuner;
        services::RealTimeFrictionAndInertiaEstimator onlineMechEstimator;
        services::RealTimeResistanceAndInductanceEstimator onlineElecEstimator;
        std::optional<std::reference_wrapper<services::MechanicalParametersIdentification>> mechIdentPtr;
    };
}
