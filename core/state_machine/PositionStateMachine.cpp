#include "core/state_machine/PositionStateMachine.hpp"
#include <cassert>

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
        , focController(hardware.inverter, hardware.encoder, outerLoopArgs.maxCurrent, outerLoopArgs.baseFrequency, outerLoopArgs.lowPriorityInterrupt)
        , pidAutoTuner(focController)
        , speedAutoTuner(focController)
        , onlineMechEstimator(services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, outerLoopArgs.baseFrequency)
        , onlineElecEstimator(services::RealTimeResistanceAndInductanceEstimator::defaultForgettingFactor, outerLoopArgs.baseFrequency)
        , mechIdentPtr(calibServices.mechIdentOverride)
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

    foc::FocSpeedTunable& PositionStateMachine::SpeedTunable()
    {
        return focController;
    }

    foc::FocOnlineEstimableBase& PositionStateMachine::OnlineEstimable()
    {
        return focController;
    }

    foc::WithAutomaticCurrentPidGains& PositionStateMachine::GetCurrentLoopTuner()
    {
        return pidAutoTuner;
    }

    foc::WithAutomaticSpeedPidGains& PositionStateMachine::GetSpeedAutoTuner()
    {
        return speedAutoTuner;
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
        assert(mechIdentPtr.has_value());
        return mechIdentPtr->get();
    }

    void PositionStateMachine::RunPostAlignmentStep()
    {
        if (!mechIdentPtr.has_value())
            EnterFault(state_machine::FaultCode::calibrationFailed);
        else
            RunMechanicalIdentStep();
    }
}
