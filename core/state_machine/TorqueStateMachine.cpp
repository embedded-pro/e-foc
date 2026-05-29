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
        : FocStateMachineCommon(terminalAndTracer, hardware, nvm,
              calibServices.electricalIdent, calibServices.motorAlignment)
        , focController(hardware.inverter, hardware.encoder)
        , pidAutoTuner(focController)
    {
        Initialize(faultNotifier, transitionPolicy);
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

    foc::WithAutomaticCurrentPidGains& TorqueStateMachine::GetCurrentLoopTuner()
    {
        return pidAutoTuner;
    }

    void TorqueStateMachine::RunPostAlignmentStep()
    {
        OnCalibrationComplete();
    }
}
