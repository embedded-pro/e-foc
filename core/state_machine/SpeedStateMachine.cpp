#include "core/state_machine/SpeedStateMachine.hpp"

namespace application
{
    SpeedStateMachine::SpeedStateMachine(
        const TerminalAndTracer& terminalAndTracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices,
        state_machine::FaultNotifier& faultNotifier,
        state_machine::TransitionPolicy transitionPolicy,
        const OuterLoopArgs& outerLoopArgs)
        : OuterLoopStateMachine(terminalAndTracer, hardware, nvm,
              calibServices.electricalIdent, calibServices.motorAlignment,
              calibServices.mechTorqueConstant)
        , focController(hardware.inverter, hardware.encoder, outerLoopArgs.maxCurrent, outerLoopArgs.baseFrequency, outerLoopArgs.lowPriorityInterrupt, outerLoopArgs.outerLoopFrequency)
        , onlineMechEstimator(services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, outerLoopArgs.outerLoopFrequency)
        , onlineElecEstimator(services::RealTimeResistanceAndInductanceEstimator::defaultForgettingFactor, outerLoopArgs.outerLoopFrequency)
        , resolvedMechIdent(ResolveMechIdent(calibServices, ownMechIdent, focController, hardware.inverter, hardware.encoder))
    {
        focController.SetOnlineMechanicalEstimator(onlineMechEstimator);
        focController.SetOnlineElectricalEstimator(onlineElecEstimator);
        RegisterFaultHandler(faultNotifier);
        RegisterCliIfNeeded(transitionPolicy);
        CheckNvmOnBoot();
    }

    foc::FocSpeed& SpeedStateMachine::GetController()
    {
        return focController;
    }

    foc::FocBase& SpeedStateMachine::GetFoc()
    {
        return focController;
    }

    foc::Controllable& SpeedStateMachine::GetFocControl()
    {
        return focController;
    }

    foc::SpeedLoopTunable& SpeedStateMachine::SpeedTunable()
    {
        return focController;
    }

    foc::CurrentLoopTunable& SpeedStateMachine::CurrentTunable()
    {
        return focController;
    }

    services::RealTimeFrictionAndInertiaEstimator& SpeedStateMachine::GetOnlineMechEstimator()
    {
        return onlineMechEstimator;
    }

    services::RealTimeResistanceAndInductanceEstimator& SpeedStateMachine::GetOnlineElecEstimator()
    {
        return onlineElecEstimator;
    }

    services::MechanicalParametersIdentification& SpeedStateMachine::MechIdentImpl()
    {
        return resolvedMechIdent.get();
    }
}
