#pragma once

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
        const foc::FocTorque& GetController() const;

    protected:
        foc::FocBase& GetFoc() override;
        foc::Controllable& GetFocControl() override;
        void RunPostAlignmentStep() override;
        foc::CurrentLoopTunable& CurrentTunable() override;

    private:
        foc::FocTorqueController focController;
    };
}
