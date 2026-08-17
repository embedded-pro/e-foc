#include "core/state_machine/PositionStateMachine.hpp"

namespace application
{
    PositionStateMachine::PositionStateMachine(
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

    foc::FocPosition& PositionStateMachine::GetController()
    {
        return focController;
    }

    foc::FocBase& PositionStateMachine::GetFoc()
    {
        return focController;
    }

    foc::Controllable& PositionStateMachine::GetFocControl()
    {
        return focController;
    }

    foc::SpeedLoopTunable& PositionStateMachine::SpeedTunable()
    {
        return focController;
    }

    foc::CurrentLoopTunable& PositionStateMachine::CurrentTunable()
    {
        return focController;
    }

    services::RealTimeFrictionAndInertiaEstimator& PositionStateMachine::GetOnlineMechEstimator()
    {
        return onlineMechEstimator;
    }

    services::RealTimeResistanceAndInductanceEstimator& PositionStateMachine::GetOnlineElecEstimator()
    {
        return onlineElecEstimator;
    }

    services::MechanicalParametersIdentification& PositionStateMachine::MechIdentImpl()
    {
        return resolvedMechIdent.get();
    }
}
