#pragma once

#include "core/foc/implementations/WithAutomaticCurrentPidGains.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/foc/interfaces/Foc.hpp"
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
        foc::ThreePhaseInverter& inverter;
        foc::Encoder& encoder;
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

        void CmdCalibrate(const infra::Function<void(state_machine::CommandResult)>& onDone) override;
        void CmdEnable() override;
        void CmdDisable() override;
        void CmdClearFault() override;
        void CmdClearCalibration(const infra::Function<void(state_machine::CommandResult)>& onDone) override;
        void CmdEmergencyStop() override;

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
        virtual foc::WithAutomaticCurrentPidGains& GetCurrentLoopTuner() = 0;

        virtual void ApplyModeSpecificCalibration(const services::CalibrationData& data);
        virtual void PrepareForEnabled();
        virtual void RegisterModeSpecificCli(services::TerminalWithStorage& terminal);

        void EnterCalibrating();
        void EnterReady(const services::CalibrationData& data);
        void EnterEnabled();
        void EnterFault(state_machine::FaultCode code);

        void CompletePendingCommand(state_machine::CommandResult result);
        bool HasPendingCommand() const;

        void RunPolePairsStep();
        void RunResistanceAndInductanceStep();
        void RunAlignmentStep();
        void OnCalibrationComplete();

        bool IsCalibrating(state_machine::CalibrationStep expected) const;

        void ApplyCalibrationDataCommon(const services::CalibrationData& data);

        services::Tracer& GetTracer();
        foc::ThreePhaseInverter& GetInverter();
        foc::Volts GetVdc() const;
        state_machine::State& GetCurrentState();
        const state_machine::State& GetCurrentState() const;

        static constexpr float nyquistFactor = 15.0f;

    private:
        void OnNvmValidationResult(bool valid);
        void OnNvmLoadResult(services::NvmStatus status);

        services::TerminalWithStorage& terminal;
        services::Tracer& tracer;
        foc::ThreePhaseInverter& inverter;
        foc::Encoder& encoder;
        foc::Volts vdc;
        services::NonVolatileMemory& nvm;
        services::ElectricalParametersIdentification& electricalIdent;
        services::MotorAlignment& motorAlignment;

        state_machine::State currentState{ state_machine::Idle{} };
        state_machine::FaultCode lastFaultCode{ state_machine::FaultCode::hardwareFault };
        services::CalibrationData calibrationData{};
        infra::AutoResetFunction<void(state_machine::CommandResult)> pendingCommandCallback;
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
