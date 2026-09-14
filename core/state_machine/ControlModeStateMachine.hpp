#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/AlgorithmPersistence.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/FocStateMachine.hpp"
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
        notWhileEnabled
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

        TuningResult TrySetCurrentBandwidth(float bandwidth);
        TuningResult TrySetSpeedBandwidth(float bandwidth);
        TuningResult TrySetPositionBandwidth(float bandwidth);

        void SetFluxLinkage(foc::Weber fluxLinkage, const infra::Function<void(CommandResult)>& onDone);
        foc::Weber ActiveFluxLinkage() const;
        CommandResult CmdReserveExternalCalibration();
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
        foc::CurrentLoopSelectable* CurrentSelectable();
        foc::SpeedLoopSelectable* SpeedSelectable();
        foc::PositionLoopSelectable* PositionSelectable();
        const foc::CurrentLoopSelectable* CurrentSelectable() const;
        const foc::SpeedLoopSelectable* SpeedSelectable() const;
        const foc::PositionLoopSelectable* PositionSelectable() const;
        void RegisterCliCommands();
        TuningResult CheckRedesignPreconditions() const;
        void OnSaveConfigDone(services::NvmStatus status);

        const application::TerminalAndTracer terminalAndTracer;
        const application::MotorHardware hardware;
        services::NonVolatileMemory& nvm;
        const application::CalibrationServices calibServices;
        FaultNotifier& faultNotifier;
        OuterLoopArgs outerLoopArgs;

        services::ConfigData configData;
        AlgorithmPersistence algorithmPersistence;
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
