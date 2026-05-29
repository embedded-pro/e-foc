#pragma once

#include "core/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "core/foc/instantiations/FocController.hpp"
#include "core/state_machine/FocStateMachineCommon.hpp"

namespace application
{
    class TorqueStateMachine
        : public FocStateMachineCommon
    {
    public:
        TorqueStateMachine(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            state_machine::FaultNotifier& faultNotifier,
            state_machine::TransitionPolicy transitionPolicy);

        foc::FocTorque& GetController();

    protected:
        foc::FocBase& GetFoc() override;
        foc::Controllable& GetFocControl() override;
        void RunPostAlignmentStep() override;
        foc::WithAutomaticCurrentPidGains& GetCurrentLoopTuner() override;

    private:
        foc::FocTorqueController focController;
        foc::WithAutomaticCurrentPidGains pidAutoTuner;
    };
}
