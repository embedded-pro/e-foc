#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/alignment/MotorAlignment.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include "core/services/mechanical_system_ident/MechanicalParametersIdentification.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/TransitionPolicies.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/TerminalWithStorage.hpp"
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
    };

    class FocStateMachineCommon
        : public state_machine::FocStateMachineBase
    {
    public:
        const state_machine::State& CurrentState() const override;
        state_machine::FaultCode LastFaultCode() const override;
        bool HasPendingAsyncWork() const override;

        void CmdCalibrate(const infra::Function<void(state_machine::CommandResult)>& onDone) override;
        state_machine::CommandResult CmdEnable() override;
        state_machine::CommandResult CmdDisable() override;
        state_machine::CommandResult CmdClearFault() override;
        void CmdClearCalibration(const infra::Function<void(state_machine::CommandResult)>& onDone) override;
        state_machine::CommandResult CmdEmergencyStop() override;

        void RegisterReadyHandler(const infra::Function<void()>& onReady);

    protected:
        FocStateMachineCommon(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            services::ElectricalParametersIdentification& electricalIdent,
            services::MotorAlignment& motorAlignment);

        void RegisterFaultHandler(state_machine::FaultNotifier& faultNotifier);
        void RegisterCliIfNeeded(state_machine::TransitionPolicy transitionPolicy);
        void CheckNvmOnBoot();

        virtual foc::FocBase& GetFoc() = 0;
        virtual foc::Controllable& GetFocControl() = 0;
        virtual void RunPostAlignmentStep() = 0;
        virtual foc::CurrentLoopTunable& CurrentTunable() = 0;

        virtual void ApplyModeSpecificCalibration(const services::CalibrationData& data);
        virtual void PrepareForEnabled();
        virtual void RegisterModeSpecificCli(services::TerminalWithStorage& terminal);

        void EnterCalibrating();
        void EnterReady(const services::CalibrationData& data);
        void EnterEnabled();
        void EnterFault(state_machine::FaultCode code);

        void CompletePendingCommand(state_machine::CommandResult result);
        bool HasPendingCommand() const;
        bool HasValidCalibration() const;

        void RunPolePairsStep();
        void RunResistanceAndInductanceStep();
        void RunAlignmentStep();
        void OnCalibrationComplete();

        bool IsCalibrating(state_machine::CalibrationStep expected) const;

        services::Tracer& GetTracer();
        drivers::ThreePhaseInverter& GetInverter();
        foc::Volts GetVdc() const;
        state_machine::State& GetCurrentState();
        const state_machine::State& GetCurrentState() const;

        static constexpr float nyquistFactor = 15.0f;

        void ApplyElectricalCalibration(const services::CalibrationData& data);
        void ApplyElectricalModel(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs, float bandwidth);
        const services::CalibrationData& GetCalibration() const;
        foc::CurrentLoopTunings CurrentTuningsFor(float bandwidth) const;
        float DefaultCurrentLoopBandwidth() const;

    private:
        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        drivers::ThreePhaseInverter& inverter;
        drivers::Encoder& encoder;
        foc::Volts vdc;
        services::NonVolatileMemory& nvm;
        services::ElectricalParametersIdentification& electricalIdent;
        services::MotorAlignment& motorAlignment;

        state_machine::State currentState{ state_machine::Idle{} };
        state_machine::FaultCode lastFaultCode{ state_machine::FaultCode::none };
        services::CalibrationData calibrationData{};
        bool bootCheckInFlight{ false };
        infra::AutoResetFunction<void(state_machine::CommandResult)> pendingCommandCallback;
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
