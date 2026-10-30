#include "core/state_machine/TorqueStateMachine.hpp"

namespace application
{
    TorqueStateMachine::TorqueStateMachine(
        const TerminalAndTracer& terminalAndTracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices,
        state_machine::FaultNotifier& faultNotifier,
        state_machine::TransitionPolicy transitionPolicy,
        NvmActivity& nvmActivity,
        RotorReference rotorReference)
        : FocStateMachineCommon(terminalAndTracer, hardware, nvm, calibServices, nvmActivity)
        , focController(hardware.inverter, hardware.encoder, hardware.inverter.MaxCurrentSupported())
        , resolvedFluxIdent(ResolveFluxLinkageIdent(calibServices, ownFluxIdent, focController, focController, focController, hardware.inverter))
    {
        if (hardware.currentStream != nullptr)
            focController.SetCurrentStream(*hardware.currentStream);
        RegisterFaultHandler(faultNotifier);
        RegisterCliIfNeeded(transitionPolicy);
        Boot(rotorReference);
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

    services::FluxLinkageIdentification* TorqueStateMachine::FluxLinkageIdent()
    {
        return resolvedFluxIdent;
    }

    void TorqueStateMachine::RunPostAlignmentStep(state_machine::Calibrating& calibrating)
    {
        SaveCalibration(calibrating);
    }
}
