#pragma once

#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/BootSequence.hpp"
#include "core/state_machine/CalibrationContext.hpp"
#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/CommandRejections.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/FocStateMachineDependencies.hpp"
#include "core/state_machine/FocStateMachineEvents.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "core/state_machine/MaintenanceFlow.hpp"
#include "core/state_machine/ModeHooks.hpp"
#include "core/state_machine/NvmActivity.hpp"
#include "core/state_machine/OperationFlow.hpp"
#include "core/state_machine/PendingCommand.hpp"
#include "core/state_machine/TransitionPolicies.hpp"
#include "services/fsm/StateMachineTracer.hpp"

namespace application
{
    class FocStateMachineCommon
        : public state_machine::FocStateMachineBase
        , protected ModeHooks
    {
    public:
        using StateMachine = LifecycleMachine;
        using StateId = StateMachine::StateId;

        // The deepest run of nested dispatches is a calibration sequence whose identification services
        // report inline: the three steps the orchestrator announces plus the AlignmentSucceeded that
        // follows the last one, which peaks at four. frictionAndInertia is not among them, because the
        // mode hook sets that step directly rather than dispatching it, and the events that follow the
        // alignment are pushed only once the queue has drained. The remaining slots absorb a command
        // chained from a completion callback. Overflow is a really_assert, and each slot costs
        // sizeof(Event), so this is budgeted against the 32 KB targets rather than rounded up.
        static constexpr std::size_t eventQueueDepth{ 6 };

        ~FocStateMachineCommon() override = default;

        const state_machine::State& CurrentState() const override;
        state_machine::FaultCode LastFaultCode() const override;
        bool HasPendingAsyncWork() const override;
        bool HasPartialCalibration() const override;

        void CmdCalibrate(const infra::Function<void(state_machine::CommandResult)>& onDone) override;
        state_machine::CommandResult CmdEnable() override;
        state_machine::CommandResult CmdDisable() override;
        state_machine::CommandResult CmdClearFault() override;
        void CmdClearCalibration(const infra::Function<void(state_machine::CommandResult)>& onDone) override;
        state_machine::CommandResult CmdEmergencyStop() override;

        void CmdReAlign(const infra::Function<void(state_machine::CommandResult)>& onDone);
        state_machine::CommandResult CmdReserveExternalCalibration();
        void CmdCompleteExternalCalibration(const services::CalibrationData& data, const infra::Function<void(state_machine::CommandResult)>& onDone);
        void CmdSetFluxLinkage(foc::Weber fluxLinkage, const infra::Function<void(state_machine::CommandResult)>& onDone);
        foc::Weber ActiveFluxLinkage() const;

        void RegisterReadyHandler(const infra::Function<void()>& onReady);
        const StateMachine& TransitionTable() const;

    protected:
        FocStateMachineCommon(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices);

        void RegisterFaultHandler(state_machine::FaultNotifier& faultNotifier);
        void ReleaseExternalResources();
        void RegisterCliIfNeeded(state_machine::TransitionPolicy transitionPolicy);
        void Boot();

        void ProvisionalControlSuperseded() override;
        void RestoreControlAfterProvisionalIdentification() override;
        void MarkProvisionalControlApplied();

        void ApplyModeSpecificCalibration(const services::CalibrationData& data) override;
        bool HasValidModeSpecificCalibration(const services::CalibrationData& data) const override;
        void PrepareForEnabled() override;
        void AbortModeSpecificServices() override;
        bool HasModeSpecificWorkPending() const override;
        void RegisterModeSpecificCli(services::TerminalWithStorage& terminal) override;

        void SaveCalibration(state_machine::Calibrating& calibrating);
        services::DispatchResult Dispatch(const state_machine::Event& event);

        services::Tracer& GetTracer();
        drivers::ThreePhaseInverter& GetInverter();
        foc::Volts GetVdc() const;
        const services::CalibrationData& GetCalibration() const;
        foc::Weber EffectiveFluxLinkage(const services::CalibrationData& data) const;
        bool ApplyElectricalModel(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs, float bandwidth, foc::Weber fluxLinkage);

    private:
        static state_machine::CommandResult ToCommandResult(services::DispatchResult result);

    private:
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        CalibrationContext calibrationContext;
        NvmActivity nvmActivity;
        PendingCommand pendingCommand;
        LifecycleEnvironment environment;
        CalibrationFlow calibration;
        MaintenanceFlow maintenance;
        BootSequence boot;
        OperationFlow operation;
        LifecycleContext context;
        StateMachine::WithStorage<eventQueueDepth> stateMachine;
        services::StateMachineTracer<state_machine::State, state_machine::Event> stateMachineTracer;
        CommandRejections commandRejections;
        bool provisionalControlApplied{ false };
    };
}
