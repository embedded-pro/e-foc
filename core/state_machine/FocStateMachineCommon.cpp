#include "core/state_machine/FocStateMachineCommon.hpp"
#include <bit>
#include <numbers>

namespace application
{
    FocStateMachineCommon::FocStateMachineCommon(
        const TerminalAndTracer& terminalAndTracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        services::ElectricalParametersIdentification& electricalIdent,
        services::MotorAlignment& motorAlignment)
        : terminal(terminalAndTracer.terminal)
        , tracer(terminalAndTracer.tracer)
        , inverter(hardware.inverter)
        , encoder(hardware.encoder)
        , vdc(hardware.vdc)
        , nvm(nvm)
        , electricalIdent(electricalIdent)
        , motorAlignment(motorAlignment)
    {}

    void FocStateMachineCommon::RegisterFaultHandler(state_machine::FaultNotifier& faultNotifier)
    {
        faultNotifier.Register([this](state_machine::FaultCode code)
            {
                EnterFault(code);
            });
    }

    void FocStateMachineCommon::RegisterCliIfNeeded(state_machine::TransitionPolicy transitionPolicy)
    {
        if (transitionPolicy != state_machine::TransitionPolicy::Cli)
            return;

        RegisterLifecycleCliCommands(terminal, [this]() -> state_machine::FocStateMachineBase&
            {
                return *this;
            });
        RegisterModeSpecificCli(terminal);
    }

    const state_machine::State& FocStateMachineCommon::CurrentState() const
    {
        return currentState;
    }

    state_machine::FaultCode FocStateMachineCommon::LastFaultCode() const
    {
        return lastFaultCode;
    }

    void FocStateMachineCommon::CmdCalibrate(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (!state_machine::IsStopped(currentState) || HasPendingCommand())
        {
            onDone(state_machine::CommandResult::rejected);
            return;
        }

        pendingCommandCallback = onDone;
        EnterCalibrating();
    }

    state_machine::CommandResult FocStateMachineCommon::CmdEnable()
    {
        if (!std::holds_alternative<state_machine::Ready>(currentState))
            return state_machine::CommandResult::rejected;

        EnterEnabled();
        return state_machine::CommandResult::ok;
    }

    state_machine::CommandResult FocStateMachineCommon::CmdDisable()
    {
        if (!std::holds_alternative<state_machine::Enabled>(currentState))
            return state_machine::CommandResult::rejected;

        GetFocControl().Stop();
        EnterReady(calibrationData);
        return state_machine::CommandResult::ok;
    }

    state_machine::CommandResult FocStateMachineCommon::CmdClearFault()
    {
        if (!std::holds_alternative<state_machine::Fault>(currentState))
            return state_machine::CommandResult::rejected;

        tracer.Trace() << "[SM] Fault cleared";

        currentState = state_machine::Idle{};
        return state_machine::CommandResult::ok;
    }

    void FocStateMachineCommon::CmdClearCalibration(const infra::Function<void(state_machine::CommandResult)>& onDone)
    {
        if (!state_machine::IsStopped(currentState) || HasPendingCommand())
        {
            onDone(state_machine::CommandResult::rejected);
            return;
        }

        pendingCommandCallback = onDone;
        nvm.InvalidateCalibration([this](services::NvmStatus status)
            {
                // Drop stale callbacks: a fault, mode-switch or other intervening
                // command may have already moved the SM elsewhere and cleared the
                // pending callback. Acting now would corrupt state.
                if (!HasPendingCommand() || !state_machine::IsStopped(currentState))
                    return;

                // A busy device means nothing was written; reject without faulting.
                if (status == services::NvmStatus::Busy)
                {
                    CompletePendingCommand(state_machine::CommandResult::rejected);
                    return;
                }

                if (status != services::NvmStatus::Ok)
                {
                    CompletePendingCommand(state_machine::CommandResult::nvmFailed);
                    EnterFault(state_machine::FaultCode::hardwareFault);
                }
                else
                {
                    tracer.Trace() << "[SM] Calibration invalidated in NVM";
                    calibrationData = services::CalibrationData{};
                    currentState = state_machine::Idle{};
                    CompletePendingCommand(state_machine::CommandResult::ok);
                }
            });
    }

    state_machine::CommandResult FocStateMachineCommon::CmdEmergencyStop()
    {
        GetFocControl().Stop();

        tracer.Trace() << "[SM] Emergency stop";

        const bool wasActive = std::holds_alternative<state_machine::Enabled>(currentState) ||
                               std::holds_alternative<state_machine::Calibrating>(currentState);

        CompletePendingCommand(state_machine::CommandResult::abortedByFault);

        if (std::holds_alternative<state_machine::Fault>(currentState))
            return state_machine::CommandResult::ok;

        if (wasActive)
            currentState = state_machine::Idle{};

        return state_machine::CommandResult::ok;
    }

    void FocStateMachineCommon::ApplyModeSpecificCalibration(const services::CalibrationData& /*data*/)
    {}

    void FocStateMachineCommon::PrepareForEnabled()
    {}

    void FocStateMachineCommon::RegisterModeSpecificCli(services::TerminalWithStorage& /*terminal*/)
    {}

    void FocStateMachineCommon::EnterCalibrating()
    {
        tracer.Trace() << "[SM] Entering Calibrating";
        currentState = state_machine::Calibrating{};
        RunPolePairsStep();
    }

    void FocStateMachineCommon::RegisterReadyHandler(const infra::Function<void()>& onReady)
    {
        readyHandler = onReady;
    }

    void FocStateMachineCommon::EnterReady(const services::CalibrationData& data)
    {
        tracer.Trace() << "[SM] Entering Ready";
        calibrationData = data;
        currentState = state_machine::Ready{ data };

        if (readyHandler != nullptr)
            readyHandler();
    }

    void FocStateMachineCommon::EnterEnabled()
    {
        tracer.Trace() << "[SM] Entering Enabled";
        PrepareForEnabled();
        GetFocControl().Start();
        currentState = state_machine::Enabled{};
    }

    void FocStateMachineCommon::EnterFault(state_machine::FaultCode code)
    {
        tracer.Trace() << "[SM] Entering Fault";

        if (std::holds_alternative<state_machine::Enabled>(currentState) ||
            std::holds_alternative<state_machine::Calibrating>(currentState))
            GetFocControl().Stop();

        lastFaultCode = code;
        currentState = state_machine::Fault{ code };
        CompletePendingCommand(state_machine::CommandResult::abortedByFault);
    }

    void FocStateMachineCommon::CompletePendingCommand(state_machine::CommandResult result)
    {
        if (pendingCommandCallback != nullptr)
            pendingCommandCallback(result);
    }

    bool FocStateMachineCommon::HasPendingCommand() const
    {
        return pendingCommandCallback != nullptr;
    }

    void FocStateMachineCommon::RunPolePairsStep()
    {
        tracer.Trace() << "[SM] Identifying pole pairs";
        auto& calibrating = std::get<state_machine::Calibrating>(currentState);
        calibrating.step = state_machine::CalibrationStep::polePairs;

        electricalIdent.EstimateNumberOfPolePairs({}, [this](std::optional<std::size_t> result)
            {
                if (!IsCalibrating(state_machine::CalibrationStep::polePairs))
                    return;

                if (!result.has_value())
                {
                    CompletePendingCommand(state_machine::CommandResult::calibrationFailed);
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                }
                else
                {
                    auto& cal = std::get<state_machine::Calibrating>(currentState);
                    cal.pendingData.polePairs = static_cast<uint8_t>(*result);
                    RunResistanceAndInductanceStep();
                }
            });
    }

    void FocStateMachineCommon::RunResistanceAndInductanceStep()
    {
        tracer.Trace() << "[SM] Identifying resistance and inductance";
        auto& calibrating = std::get<state_machine::Calibrating>(currentState);
        calibrating.step = state_machine::CalibrationStep::resistanceAndInductance;

        electricalIdent.EstimateResistanceAndInductance({},
            [this](std::optional<foc::Ohm> r, std::optional<foc::MilliHenry> l)
            {
                if (!IsCalibrating(state_machine::CalibrationStep::resistanceAndInductance))
                    return;

                if (!r || !l)
                {
                    CompletePendingCommand(state_machine::CommandResult::calibrationFailed);
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                }
                else
                {
                    auto& cal = std::get<state_machine::Calibrating>(currentState);
                    cal.pendingData.rPhase = r->Value();
                    cal.pendingData.lD = l->Value();
                    cal.pendingData.lQ = l->Value();
                    RunAlignmentStep();
                }
            });
    }

    void FocStateMachineCommon::RunAlignmentStep()
    {
        tracer.Trace() << "[SM] Aligning motor";
        auto& calibrating = std::get<state_machine::Calibrating>(currentState);
        calibrating.step = state_machine::CalibrationStep::alignment;
        const auto polePairs = calibrating.pendingData.polePairs;

        motorAlignment.ForceAlignment(polePairs, {},
            [this](std::optional<foc::Radians> angle)
            {
                if (!IsCalibrating(state_machine::CalibrationStep::alignment))
                    return;

                if (!angle)
                {
                    CompletePendingCommand(state_machine::CommandResult::calibrationFailed);
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                }
                else
                {
                    auto& cal = std::get<state_machine::Calibrating>(currentState);
                    cal.pendingData.encoderZeroOffset = std::bit_cast<int32_t>(angle->Value());
                    RunPostAlignmentStep();
                }
            });
    }

    void FocStateMachineCommon::OnCalibrationComplete()
    {
        if (!std::holds_alternative<state_machine::Calibrating>(currentState))
            return;

        auto pendingData = std::get<state_machine::Calibrating>(currentState).pendingData;

        nvm.SaveCalibration(pendingData,
            [this](services::NvmStatus status)
            {
                if (!std::holds_alternative<state_machine::Calibrating>(currentState))
                    return;

                if (status != services::NvmStatus::Ok)
                {
                    CompletePendingCommand(state_machine::CommandResult::nvmFailed);
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                }
                else
                {
                    auto data = std::get<state_machine::Calibrating>(currentState).pendingData;
                    encoder.Set(foc::Radians{ std::bit_cast<float>(data.encoderZeroOffset) });
                    ApplyElectricalCalibration(data);
                    ApplyModeSpecificCalibration(data);
                    EnterReady(data);
                    CompletePendingCommand(state_machine::CommandResult::ok);
                }
            });
    }

    bool FocStateMachineCommon::IsCalibrating(state_machine::CalibrationStep expected) const
    {
        if (!std::holds_alternative<state_machine::Calibrating>(currentState))
            return false;
        return std::get<state_machine::Calibrating>(currentState).step == expected;
    }

    void FocStateMachineCommon::CheckNvmOnBoot()
    {
        nvm.IsCalibrationValid([this](bool valid)
            {
                if (!std::holds_alternative<state_machine::Idle>(currentState))
                    return;

                if (!valid)
                    tracer.Trace() << "[SM] NVM invalid, starting in Idle";
                else
                    nvm.LoadCalibration(calibrationData, [this](services::NvmStatus status)
                        {
                            if (!std::holds_alternative<state_machine::Idle>(currentState))
                                return;

                            if (status != services::NvmStatus::Ok)
                                tracer.Trace() << "[SM] NVM load failed, starting in Idle";
                            else
                            {
                                encoder.Set(foc::Radians{ std::bit_cast<float>(calibrationData.encoderZeroOffset) });
                                ApplyElectricalCalibration(calibrationData);
                                ApplyModeSpecificCalibration(calibrationData);
                                EnterReady(calibrationData);
                            }
                        });
            });
    }

    services::Tracer& FocStateMachineCommon::GetTracer()
    {
        return tracer;
    }

    drivers::ThreePhaseInverter& FocStateMachineCommon::GetInverter()
    {
        return inverter;
    }

    foc::Volts FocStateMachineCommon::GetVdc() const
    {
        return vdc;
    }

    state_machine::State& FocStateMachineCommon::GetCurrentState()
    {
        return currentState;
    }

    const state_machine::State& FocStateMachineCommon::GetCurrentState() const
    {
        return currentState;
    }

    float FocStateMachineCommon::DefaultCurrentLoopBandwidth() const
    {
        return (static_cast<float>(inverter.BaseFrequency().Value()) / nyquistFactor) * 2.0f * std::numbers::pi_v<float>;
    }

    foc::CurrentLoopTunings FocStateMachineCommon::CurrentTuningsFor(float bandwidth) const
    {
        auto tunings = foc::CurrentLoopTunings{};
        tunings.bandwidth = bandwidth > 0.0f ? bandwidth : DefaultCurrentLoopBandwidth();
        return tunings;
    }

    void FocStateMachineCommon::ApplyElectricalCalibration(const services::CalibrationData& data)
    {
        ApplyElectricalModel(foc::Ohm{ data.rPhase }, foc::MilliHenry{ data.lD }, data.polePairs, data.currentLoopBandwidth);
    }

    void FocStateMachineCommon::ApplyElectricalModel(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs, float bandwidth)
    {
        GetFoc().Configure(foc::MotorModelParameters{
            resistance,
            inductance,
            foc::Weber{ 0.0f },
            vdc,
            inverter.BaseFrequency(),
            polePairs });

        CurrentTunable().SetCurrentTunings(CurrentTuningsFor(bandwidth));
    }

    const services::CalibrationData& FocStateMachineCommon::GetCalibration() const
    {
        return calibrationData;
    }
}
