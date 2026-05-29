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
        foc::Ampere maxCurrent,
        hal::Hertz baseFrequency,
        foc::LowPriorityInterrupt& lowPriorityInterrupt)
        : OuterLoopStateMachine(terminalAndTracer, hardware, nvm,
              calibServices.electricalIdent, calibServices.motorAlignment,
              calibServices.mechTorqueConstant)
        , focController(hardware.inverter, hardware.encoder, maxCurrent, baseFrequency, lowPriorityInterrupt)
        , pidAutoTuner(focController)
        , speedAutoTuner(focController)
        , onlineMechEstimator(services::RealTimeFrictionAndInertiaEstimator::defaultForgettingFactor, baseFrequency)
        , onlineElecEstimator(services::RealTimeResistanceAndInductanceEstimator::defaultForgettingFactor, baseFrequency)
        , ownMechIdent{}
        , resolvedMechIdent(ResolveMechIdent(calibServices, ownMechIdent, focController, hardware.inverter, hardware.encoder))
    {
        OnlineEstimable().SetOnlineMechanicalEstimator(onlineMechEstimator);
        OnlineEstimable().SetOnlineElectricalEstimator(onlineElecEstimator);
        Initialize(faultNotifier, transitionPolicy);
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

    foc::FocSpeedTunable& SpeedStateMachine::SpeedTunable()
    {
        return focController;
    }

    foc::FocOnlineEstimableBase& SpeedStateMachine::OnlineEstimable()
    {
        return focController;
    }

    foc::WithAutomaticCurrentPidGains& SpeedStateMachine::GetCurrentLoopTuner()
    {
        return pidAutoTuner;
    }

    foc::WithAutomaticSpeedPidGains& SpeedStateMachine::GetSpeedAutoTuner()
    {
        return speedAutoTuner;
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
        foc::ThreePhaseInverter& inverter,
        foc::Encoder& encoder)
    {
        if (calibServices.mechIdentOverride.has_value())
            return calibServices.mechIdentOverride->get();

        ownMechIdent.emplace(static_cast<foc::FocSpeed&>(focController), inverter, encoder);
        return *ownMechIdent;
    }
}
