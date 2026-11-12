#pragma once

#include "core/services/flux_linkage_ident/FluxLinkageIdentificationImpl.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/BootSequence.hpp"
#include "core/state_machine/CalibrationContext.hpp"
#include "core/state_machine/CalibrationFlow.hpp"
#include "core/state_machine/CommandRejections.hpp"
#include "core/state_machine/CommissioningCommands.hpp"
#include "core/state_machine/DefaultModeHooks.hpp"
#include "core/state_machine/FluxLinkageIdentResolution.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/FocStateMachineDependencies.hpp"
#include "core/state_machine/FocStateMachineEvents.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "core/state_machine/MaintenanceFlow.hpp"
#include "core/state_machine/NvmActivity.hpp"
#include "core/state_machine/OperationFlow.hpp"
#include "core/state_machine/PendingCommand.hpp"
#include "services/fsm/StateMachineTracer.hpp"

namespace application
{
    class FocStateMachineCommon
        : public state_machine::FocStateMachineBase
        , protected DefaultModeHooks
    {
    public:
        using StateMachine = LifecycleMachine;
        using StateId = StateMachine::StateId;

        // Budgeted per slot (sizeof(Event)) against the deepest nested dispatch; see state-machine.md.
        static constexpr std::size_t eventQueueDepth{ 6 };

        const state_machine::State& CurrentState() const override;
        state_machine::FaultCode LastFaultCode() const override;
        bool HasPendingAsyncWork() const override;
        bool HasPartialCalibration() const override;

        void CmdCalibrate(const infra::Function<void(state_machine::CommandResult)>& onDone) override;
        state_machine::CommandResult CmdEnable() override;
        void CmdEnableAsync(const infra::Function<void(state_machine::CommandResult)>& onDone) override;
        state_machine::CommandResult CmdDisable() override;
        state_machine::CommandResult CmdClearFault() override;
        void CmdClearCalibration(const infra::Function<void(state_machine::CommandResult)>& onDone) override;
        state_machine::CommandResult CmdEmergencyStop() override;
        state_machine::CommandResult CmdQuickStop() override;
        void SetQuickStopDeceleration(foc::RadiansPerSecondSquared deceleration);
        foc::RadiansPerSecondSquared QuickStopDeceleration() const override;

        void RequireHoming(bool required);
        CommissioningCommands& Commissioning();
        const CalibrationContext& Calibration() const;

        void RegisterReadyHandler(const infra::Function<void()>& onReady);
        void RegisterFaultEnteredHandler(const infra::Function<void(state_machine::FaultCode)>& onFaultEntered);
        const StateMachine& TransitionTable() const;

    protected:
        FocStateMachineCommon(services::Tracer& tracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            NvmActivity& nvmActivity);
        ~FocStateMachineCommon() = default;

        void RegisterFaultHandler(state_machine::FaultNotifier& faultNotifier);
        void ReleaseExternalResources();
        void Boot(RotorReference rotorReference);

        void ProvisionalControlSuperseded() override;
        void RestoreControlAfterProvisionalIdentification() override;
        void MarkProvisionalControlApplied();
        bool ApplyProvisionalCurrentControl(const services::CalibrationData& pending) override;

        void SaveCalibration(state_machine::Calibrating& calibrating);
        services::DispatchResult Dispatch(const state_machine::Event& event);

        services::Tracer& GetTracer();
        const services::CalibrationData& GetCalibration() const;
        foc::Weber EffectiveFluxLinkage(const services::CalibrationData& data) const;
        bool ApplyElectricalModel(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs, foc::Weber fluxLinkage, foc::Ampere currentNoise);

    private:
        services::Tracer& tracer;
        CalibrationContext calibrationContext;
        NvmActivity& nvmActivity;
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
        CommissioningCommands commissioning;
        bool provisionalControlApplied{ false };
        foc::RadiansPerSecondSquared quickStopDeceleration{ 0.0f };
    };
}
