#pragma once

#include "core/foc/instantiations/FocController.hpp"
#include "core/services/electrical_system_ident/RealTimeResistanceAndInductanceEstimator.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentificationImpl.hpp"
#include "core/services/mechanical_system_ident/RealTimeFrictionAndInertiaEstimator.hpp"
#include "core/state_machine/OuterLoopStateMachine.hpp"
#include <functional>
#include <optional>

namespace application
{
    // Speed and position modes differ only in which cascade they drive; the estimators, the
    // mechanical identification fallback and the accessor wiring are the same for both.
    template<class Cascade, class ControlInterface>
    class OuterLoopStateMachineFor
        : public OuterLoopStateMachine
    {
    public:
        OuterLoopStateMachineFor(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            state_machine::FaultNotifier& faultNotifier,
            state_machine::TransitionPolicy transitionPolicy,
            const OuterLoopArgs& outerLoopArgs)
            : OuterLoopStateMachine(terminalAndTracer, hardware, nvm, calibServices)
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

        ControlInterface& GetController()
        {
            return focController;
        }

        const ControlInterface& GetController() const
        {
            return focController;
        }

    protected:
        foc::FocBase& GetFoc() override
        {
            return focController;
        }

        foc::Controllable& GetFocControl() override
        {
            return focController;
        }

        foc::SpeedLoopTunable& SpeedTunable() override
        {
            return focController;
        }

        foc::CurrentLoopTunable& CurrentTunable() override
        {
            return focController;
        }

        services::RealTimeFrictionAndInertiaEstimator& GetOnlineMechEstimator() override
        {
            return onlineMechEstimator;
        }

        services::RealTimeResistanceAndInductanceEstimator& GetOnlineElecEstimator() override
        {
            return onlineElecEstimator;
        }

        services::MechanicalParametersIdentification& MechIdentImpl() override
        {
            return resolvedMechIdent.get();
        }

    private:
        Cascade focController;
        services::RealTimeFrictionAndInertiaEstimator onlineMechEstimator;
        services::RealTimeResistanceAndInductanceEstimator onlineElecEstimator;
        std::optional<services::MechanicalParametersIdentificationImpl> ownMechIdent;
        std::reference_wrapper<services::MechanicalParametersIdentification> resolvedMechIdent;
    };
}
