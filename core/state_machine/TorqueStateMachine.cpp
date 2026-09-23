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
        , focController(hardware.inverter, hardware.encoder, hardware.inverter.MaxCurrentSupported())
    {
        RegisterFaultHandler(faultNotifier);
        RegisterCliIfNeeded(transitionPolicy);
        Boot();
    }

    TorqueStateMachine::~TorqueStateMachine()
    {
        ReleaseExternalResources();
    }

    foc::FocTorque& TorqueStateMachine::GetController()
    {
        return focController;
    }

    const foc::FocTorque& TorqueStateMachine::GetController() const
    {
        return focController;
    }

    foc::MotionObservation TorqueStateMachine::ObserveMotion() const
    {
        return focController.ObserveMotion();
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

    void TorqueStateMachine::RunPostAlignmentStep(state_machine::Calibrating& calibrating)
    {
        SaveCalibration(calibrating);
    }
}
