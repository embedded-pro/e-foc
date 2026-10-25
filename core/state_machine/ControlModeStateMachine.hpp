#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/AlgorithmPersistence.hpp"
#include "core/state_machine/ConfigWriter.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/NvmActivity.hpp"
#include "core/state_machine/OuterLoopStateMachine.hpp"
#include "core/state_machine/PositionStateMachine.hpp"
#include "core/state_machine/SpeedStateMachine.hpp"
#include "core/state_machine/TorqueStateMachine.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>
#include <optional>
#include <variant>

namespace state_machine
{
    enum class TuningResult : uint8_t
    {
        ok,
        outOfRange,
        wrongMode,
        notStopped
    };

    enum class SettingResult : uint8_t
    {
        ok,
        busy,
        invalidState,
        wrongMode,
        outOfRange,
        invalidParameters,
        nvmFailed,
        aborted
    };

    class ControlModeStateMachine
    {
    public:
        using OuterLoopArgs = application::OuterLoopArgs;

        ControlModeStateMachine(
            const application::TerminalAndTracer& terminalAndTracer,
            const application::MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const application::CalibrationServices& calibServices,
            FaultNotifier& faultNotifier,
            const services::ConfigData& configData,
            OuterLoopArgs outerLoopArgs);

        void Select(ControlMode mode, const infra::Function<void(SelectResult)>& onDone);

        ControlMode Active() const;
        FocStateMachineBase& ActiveStateMachine();
        const FocStateMachineBase& ActiveStateMachine() const;

        bool TrySetTorque(foc::IdAndIqPoint setpoint);
        bool TrySetSpeed(foc::RadiansPerSecond setpoint);
        bool TrySetPosition(foc::Radians setpoint);

        void ApplyOnlineEstimates();

        TuningResult TrySetCurrentBandwidth(float bandwidth);
        TuningResult TrySetSpeedBandwidth(float bandwidth);
        TuningResult TrySetPositionBandwidth(float bandwidth);

        using OnSetting = infra::Function<void(SettingResult)>;

        foc::CurrentLoopTunings StoredCurrentTunings() const;
        foc::SpeedLoopTunings StoredSpeedTunings() const;
        foc::PositionLoopTunings StoredPositionTunings() const;
        foc::CurrentLoopTunings EffectiveCurrentTunings() const;
        std::optional<foc::SpeedLoopTunings> EffectiveSpeedTunings() const;
        std::optional<foc::PositionLoopTunings> EffectivePositionTunings() const;
        void SetCurrentTunings(const foc::CurrentLoopTunings& tunings, const OnSetting& onDone);
        void SetSpeedTunings(const foc::SpeedLoopTunings& tunings, const OnSetting& onDone);
        void SetPositionTunings(const foc::PositionLoopTunings& tunings, const OnSetting& onDone);

        void SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm, const OnSetting& onDone);
        void SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm, const OnSetting& onDone);
        void SelectPositionAlgorithm(foc::PositionAlgorithm algorithm, const OnSetting& onDone);

        void SetMotorParameter(services::MotorParameter parameter, float value, const OnSetting& onDone);
        const services::CalibrationData& CalibrationRecord() const;
        foc::Weber DefaultFluxLinkage() const;

        services::ConfigData& Config();
        const services::ConfigData& Config() const;
        void PersistConfig(const infra::Function<void(services::NvmStatus)>& onDone);
        bool HasPendingConfigWrite() const;
        bool HasPendingConfigPersist() const;

        foc::Weber ActiveFluxLinkage() const;
        CommandResult CmdReserveExternalCalibration();
        CommandResult CmdReleaseExternalCalibration();
        void CmdCompleteExternalCalibration(const services::CalibrationData& data,
            const infra::Function<void(CommandResult)>& onDone);
        void CmdReAlign(const infra::Function<void(CommandResult)>& onDone);
        std::optional<services::CalibrationData> ActiveCalibrationData() const;

        foc::SelectResult SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm);
        foc::SelectResult SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm);
        foc::SelectResult SelectPositionAlgorithm(foc::PositionAlgorithm algorithm);
        foc::CurrentAlgorithm ActiveCurrentAlgorithm() const;
        foc::SpeedAlgorithm ActiveSpeedAlgorithm() const;
        foc::PositionAlgorithm ActivePositionAlgorithm() const;

        application::FocStateMachineCommon& ActiveCommon();
        const application::FocStateMachineCommon& ActiveCommon() const;
        application::OuterLoopStateMachine* ActiveOuterLoop();

    private:
        void Activate(ControlMode mode);
        void AttachAlgorithmRestore(application::FocStateMachineCommon& stateMachine);
        foc::CurrentLoopTunable* CurrentTunable();
        foc::SpeedLoopTunable* SpeedTunable();
        foc::PositionLoopTunable* PositionTunable();
        const application::OuterLoopStateMachine* ActiveOuterLoop() const;
        foc::CurrentLoopSelectable* CurrentSelectable();
        foc::SpeedLoopSelectable* SpeedSelectable();
        foc::PositionLoopSelectable* PositionSelectable();
        const foc::CurrentLoopSelectable* CurrentSelectable() const;
        const foc::SpeedLoopSelectable* SpeedSelectable() const;
        const foc::PositionLoopSelectable* PositionSelectable() const;
        void RegisterCliCommands();
        TuningResult CheckRedesignPreconditions() const;
        void OnSaveConfigDone(services::NvmStatus status);
        void OnRollbackSaveConfigDone(services::NvmStatus status);
        bool CanActivatePendingSelect() const;
        SettingResult SettingGate() const;
        void ApplyOperatorTunings();
        void StoreAndPersist(const OnSetting& onDone);
        void CompleteSetting(SettingResult result);
        static SettingResult ToSettingResult(foc::SelectResult result);
        static SettingResult ToSettingResult(CommandResult result);

        const application::TerminalAndTracer terminalAndTracer;
        const application::MotorHardware hardware;
        services::NonVolatileMemory& nvm;
        const application::CalibrationServices calibServices;
        FaultNotifier& faultNotifier;
        OuterLoopArgs outerLoopArgs;

        services::ConfigData configData;
        application::NvmActivity nvmActivity;
        ConfigWriter configWriter;
        AlgorithmPersistence algorithmPersistence;
        OnSetting pendingSettingCallback;
        ControlMode pendingSelectMode{ ControlMode::torque };
        infra::AutoResetFunction<void(SelectResult)> pendingSelectCallback;
        uint8_t previousDefaultControlMode{ 0 };

        std::variant<std::monostate,
            application::TorqueStateMachine,
            application::SpeedStateMachine,
            application::PositionStateMachine>
            activeSm;
    };
}
