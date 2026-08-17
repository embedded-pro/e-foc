#include "core/state_machine/TorqueStateMachine.hpp"

namespace application
{
    TorqueStateMachine::TorqueStateMachine(
        const TerminalAndTracer& terminalAndTracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices,
        state_machine::FaultNotifier& faultNotifier,
        state_machine::TransitionPolicy transitionPolicy)
        : FocStateMachineCommon(terminalAndTracer, hardware, nvm, calibServices)
        , focController(hardware.inverter, hardware.encoder)
    {
        RegisterFaultHandler(faultNotifier);
        RegisterCliIfNeeded(transitionPolicy);
        CheckNvmOnBoot();
    }

    foc::FocTorque& TorqueStateMachine::GetController()
    {
        return focController;
    }

    foc::FocBase& TorqueStateMachine::GetFoc()
    {
        return focController;
    }

    foc::Controllable& TorqueStateMachine::GetFocControl()
    {
        return focController;
    }

    foc::CurrentLoopTunable& TorqueStateMachine::CurrentTunable()
    {
        return focController;
    }

    void TorqueStateMachine::RunPostAlignmentStep()
    {
        OnCalibrationComplete();
    }
}
