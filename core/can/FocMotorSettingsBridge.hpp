#pragma once

#include "core/can/FocMotorCategoryServer.hpp"
#include "core/platform_abstraction/PlatformDiagnostics.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace can
{
    class FocMotorSettingsBridge
    {
    public:
        FocMotorSettingsBridge(FocMotorCategoryServer& server, state_machine::ControlModeStateMachine& controlMode,
            application::PlatformDiagnostics& diagnostics, FocResetCause resetCause);

        void SetLegacyBandwidth(uint8_t messageId, FocParameter parameter, float bandwidth, const infra::Function<void()>& onDone);
        void SetEncoderResolution(uint32_t resolution, const infra::Function<void()>& onDone);
        void ConfigureTelemetryRate(uint32_t rateHz, const infra::Function<void()>& onDone);
        void SelectAlgorithm(FocControlLoop loop, uint8_t algorithm, const infra::Function<void(const FocAlgorithms&)>& onDone);
        void SetParameter(FocParameter parameter, float value, const infra::Function<void(FocParameterValue)>& onDone);
        FocAlgorithms ActiveAlgorithms() const;
        FocParameterValue HeldValue(FocParameter parameter) const;
        void ClearDiagnostics();

    private:
        bool SettingOutstanding() const;
        void SetLoopSetting(FocParameter parameter, float value, const state_machine::ControlModeStateMachine::OnSetting& completed);
        void SetStartupSetting(FocParameter parameter, uint32_t value);
        void CompleteAlgorithmSelection(state_machine::SettingResult result);
        void CompleteParameter(state_machine::SettingResult result);
        void CompleteStoredCommand(uint8_t commandId, state_machine::SettingResult result);
        void ReportSettingFailure(uint8_t commandId, state_machine::SettingResult result);
        FocParameterValue HeldLoopSetting(FocParameter parameter) const;
        FocParameterValue HeldMotorParameter(FocParameter parameter) const;
        FocParameterValue HeldCalibrationReading(FocParameter parameter) const;
        FocParameterValue HeldOnlineEstimate(FocParameter parameter) const;
        FocParameterValue HeldLoopStatistic(FocParameter parameter) const;

        FocMotorCategoryServer& server;
        state_machine::ControlModeStateMachine& controlMode;
        application::PlatformDiagnostics& diagnostics;
        FocResetCause resetCause;

        infra::Function<void()> pendingNvmDoneCallback;
        infra::Function<void(const FocAlgorithms&)> pendingAlgorithmCallback;
        infra::Function<void(FocParameterValue)> pendingParameterCallback;
        FocParameter pendingParameter{ FocParameter::currentBandwidth };
        uint32_t previousStartupSetting{ 0 };
        uint8_t pendingLegacyCommand{ 0 };
    };
}
