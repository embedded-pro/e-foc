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
        , focController(hardware.inverter, hardware.encoder, outerLoopArgs.maxCurrent, outerLoopArgs.baseFrequency, outerLoopArgs.lowPriorityInterrupt)
        , onlineMechEstimator(services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, outerLoopArgs.baseFrequency)
        , onlineElecEstimator(services::RealTimeResistanceAndInductanceEstimator::defaultForgettingFactor, outerLoopArgs.baseFrequency)
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

    services::MechanicalParametersIdentification& SpeedStateMachine::ResolveMechIdent(
        const CalibrationServices& calibServices,
        std::optional<services::MechanicalParametersIdentificationImpl>& ownMechIdent,
        foc::FocSpeedController& focController,
        drivers::ThreePhaseInverter& inverter,
        drivers::Encoder& encoder)
    {
        if (calibServices.mechIdentOverride.has_value())
            return calibServices.mechIdentOverride->get();

        ownMechIdent.emplace(static_cast<foc::FocSpeed&>(focController), inverter, encoder);
        return *ownMechIdent;
    }
}
