#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/alignment/MotorAlignment.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/CalibrationContext.hpp"
#include "core/state_machine/CalibrationOrchestrator.hpp"
#include "core/state_machine/FaultController.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/FocStateMachineEvents.hpp"
#include "core/state_machine/TransitionPolicies.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/fsm/StateMachineTracer.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <array>
#include <functional>
#include <optional>

namespace application
{
    struct TerminalAndTracer
    {
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
    };

    struct MotorHardware
    {
        drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        foc::Volts vdc;
    };

    struct CalibrationServices
    {
        services::ElectricalParametersIdentification& electricalIdent;
        services::MotorAlignment& motorAlignment;
        std::optional<std::reference_wrapper<services::MechanicalParametersIdentification>> mechIdentOverride{ std::nullopt };
        foc::NewtonMeter mechTorqueConstant{ foc::NewtonMeter{ 0.1f } };
        foc::Weber fluxLinkage{ foc::Weber{ 0.0f } };
    };

    class FocStateMachineCommon
        : public state_machine::FocStateMachineBase
    {
    public:
        using StateMachine = services::TableStateMachine<state_machine::State, state_machine::Event, FocStateMachineCommon>;
        using StateId = StateMachine::StateId;

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

        void CmdSetFluxLinkage(foc::Weber fluxLinkage, const infra::Function<void(state_machine::CommandResult)>& onDone);
        foc::Weber ActiveFluxLinkage() const;

        void CmdReAlign(const infra::Function<void(state_machine::CommandResult)>& onDone);

        state_machine::CommandResult CmdReserveExternalCalibration();
        void CmdCompleteExternalCalibration(const services::CalibrationData& data,
            const infra::Function<void(state_machine::CommandResult)>& onDone);

        void RegisterReadyHandler(const infra::Function<void()>& onReady);

        const StateMachine& TransitionTable() const;

    protected:
        FocStateMachineCommon(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices);

        void RegisterFaultHandler(state_machine::FaultNotifier& faultNotifier);

        void ReleaseExternalResources();
        void AbortCalibrationServices();
        virtual void AbortModeSpecificServices();
        void RegisterCliIfNeeded(state_machine::TransitionPolicy transitionPolicy);
        void Boot();

        virtual foc::FocBase& GetFoc() = 0;
        virtual foc::Controllable& GetFocControl() = 0;
        virtual void RunPostAlignmentStep(state_machine::Calibrating& calibrating) = 0;
        virtual foc::CurrentLoopTunable& CurrentTunable() = 0;

        virtual void ApplyModeSpecificCalibration(const services::CalibrationData& data);
        virtual bool HasValidModeSpecificCalibration(const services::CalibrationData& data) const;
        virtual void PrepareForEnabled();
        virtual void RegisterModeSpecificCli(services::TerminalWithStorage& terminal);

        void SaveCalibration(state_machine::Calibrating& calibrating);
        bool HasValidCalibration() const;

        services::Tracer& GetTracer();
        drivers::ThreePhaseInverter& GetInverter();
        foc::Volts GetVdc() const;

        const services::CalibrationData& GetCalibration() const;
        foc::Weber EffectiveFluxLinkage(const services::CalibrationData& data) const;
        void ApplyElectricalModel(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs, float bandwidth, foc::Weber fluxLinkage);

        services::DispatchResult Dispatch(const state_machine::Event& event);

    private:
        using Transition = StateMachine::Transition;

        static StateMachine::Table Rows();
        static constexpr std::array<Transition, 12> CalibrationRows();
        static constexpr std::array<Transition, 4> CalibrationCompletionRows();
        static constexpr std::array<Transition, 2> OperationRows();
        static constexpr std::array<Transition, 10> SafetyRows();
        static constexpr std::array<Transition, 12> MaintenanceRows();
        static constexpr std::array<Transition, 2> BootRows();
        template<class Stopped>
        static constexpr std::array<Transition, 3> CalibrationEntryRows();
        template<class Active>
        static constexpr std::array<Transition, 2> EmergencyStopRows();
        template<class S>
        static constexpr Transition EmergencyStopInternalRow();
        template<class Stopped>
        static constexpr std::array<Transition, 5> MaintenanceRowsFor();
        template<class Stopped>
        static constexpr Transition FluxLinkageSavedRow();
        void RegisterEnteredHooks();

        state_machine::Calibrating BeginCalibration(const state_machine::Calibrate& command);
        state_machine::Calibrating BeginReAlign(const state_machine::ReAlign& command);
        bool IsPlausibleExternalCalibration(const state_machine::CompleteExternalCalibration& command);
        void BeginExternalCalibration(state_machine::Calibrating& calibrating, const state_machine::CompleteExternalCalibration& command);
        void StartCalibrationSequence(state_machine::Calibrating& calibrating);
        void StartAlignmentOnly(state_machine::Calibrating& calibrating);
        void OnAlignmentSucceeded(state_machine::Calibrating& calibrating, foc::Radians angle);
        void OnMechanicalParametersIdentified(state_machine::Calibrating& calibrating, const state_machine::MechanicalParametersIdentified& event);
        state_machine::Ready CompleteCalibration(const state_machine::Calibrating& calibrating);
        state_machine::Idle CompletePartialCalibration(const state_machine::Calibrating& calibrating);

        void BeginClearCalibration(const state_machine::ClearCalibration& command);
        state_machine::Idle CompleteClearCalibration();
        bool IsAcceptableFluxLinkage(const state_machine::SetFluxLinkage& command);
        void BeginSetFluxLinkage(const state_machine::SetFluxLinkage& command);
        void OnFluxLinkageSaved(services::NvmStatus status);

        state_machine::Ready BuildReady();
        state_machine::Enabled BuildEnabled();
        state_machine::Fault BuildFault(state_machine::FaultCode code, bool wasActive, state_machine::CommandResult pendingResult = state_machine::CommandResult::abortedByFault);
        state_machine::Idle StopToIdle();
        state_machine::Ready StopToReady();
        void StopWithoutTransition();
        void AbortActiveWork();
        bool IsEnableAllowed(const state_machine::Ready& ready);
        state_machine::Ready DisableToReady();
        state_machine::Ready ClearFaultToReady();
        state_machine::Idle ClearFaultToIdle();

        void ContinueBoot(bool valid);
        void FinishBoot(services::NvmStatus status);
        void OnBootValidityChecked(bool valid);
        void OnBootCalibrationLoaded(services::NvmStatus status);

        void CompleteAfterTransition(state_machine::CommandResult result);
        void CompletePendingCommand(state_machine::CommandResult result);
        bool HasPendingCommand() const;
        void OnStateEntered(StateId state);

        static state_machine::CommandResult ToCommandResult(services::DispatchResult result);

    private:
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        services::NonVolatileMemory& nvm;
        CalibrationContext calibrationContext;
        FaultController faultController;
        CalibrationOrchestrator calibrationOrchestrator;

        StateMachine::WithStorage<4> stateMachine;
        services::StateMachineTracer<state_machine::State, state_machine::Event> stateMachineTracer;

        state_machine::FaultCode lastFaultCode{ state_machine::FaultCode::none };
        bool bootCheckInFlight{ false };

        infra::AutoResetFunction<void(state_machine::CommandResult)> pendingCommandCallback;
        std::optional<state_machine::CommandResult> deferredCompletion;
        infra::Function<void()> readyHandler;
    };

    template<class GetActiveSm>
    void RegisterLifecycleCliCommands(
        services::TerminalWithStorage& terminal,
        GetActiveSm getActiveSm)
    {
        terminal.AddCommand({ { "calibrate", "cal", "Run full calibration sequence" },
            [getActiveSm](const infra::BoundedConstString&)
            {
                getActiveSm().CmdCalibrate([](state_machine::CommandResult) {});
            } });

        terminal.AddCommand({ { "align", "aln", "Re-establish rotor reference without full recalibration" },
            [getActiveSm](const infra::BoundedConstString&)
            {
                getActiveSm().CmdReAlign([](state_machine::CommandResult) {});
            } });

        terminal.AddCommand({ { "enable", "en", "Enable FOC controller" },
            [getActiveSm](const infra::BoundedConstString&)
            {
                getActiveSm().CmdEnable();
            } });

        terminal.AddCommand({ { "disable", "dis", "Disable FOC controller" },
            [getActiveSm](const infra::BoundedConstString&)
            {
                getActiveSm().CmdDisable();
            } });

        terminal.AddCommand({ { "clear_fault", "cf", "Clear fault and return to Idle" },
            [getActiveSm](const infra::BoundedConstString&)
            {
                getActiveSm().CmdClearFault();
            } });

        terminal.AddCommand({ { "clear_cal", "cc", "Clear calibration data from NVM" },
            [getActiveSm](const infra::BoundedConstString&)
            {
                getActiveSm().CmdClearCalibration([](state_machine::CommandResult) {});
            } });
    }
}
