#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/PositionStateMachine.hpp"
#include "core/state_machine/SpeedStateMachine.hpp"
#include "core/state_machine/TorqueStateMachine.hpp"
#include "core/state_machine/TransitionPolicies.hpp"
#include "infra/util/Function.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <variant>

namespace state_machine
{
    class ControlModeStateMachine
    {
    public:
        struct OuterLoopArgs
        {
            foc::Ampere maxCurrent;
            hal::Hertz baseFrequency;
            foc::LowPriorityInterrupt& lowPriorityInterrupt;
        };

        ControlModeStateMachine(
            const application::TerminalAndTracer& terminalAndTracer,
            const application::MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const application::CalibrationServices& calibServices,
            FaultNotifier& faultNotifier,
            const services::ConfigData& configData,
            OuterLoopArgs outerLoopArgs);

        void Select(ControlMode mode, const infra::Function<void(SelectResult)>& onDone);
        bool SelectInProgress() const;

        ControlMode Active() const;
        FocStateMachineBase& ActiveStateMachine();
        const FocStateMachineBase& ActiveStateMachine() const;

        bool TrySetTorque(foc::IdAndIqPoint setpoint);
        bool TrySetSpeed(foc::RadiansPerSecond setpoint);
        bool TrySetPosition(foc::Radians setpoint);

    private:
        bool IsMotorStopped() const;
        void Activate(ControlMode mode);
        void RegisterCliCommands();
        void OnSaveConfigDone(services::NvmStatus status);

        const application::TerminalAndTracer terminalAndTracer;
        const application::MotorHardware hardware;
        services::NonVolatileMemory& nvm;
        const application::CalibrationServices calibServices;
        FaultNotifier& faultNotifier;
        OuterLoopArgs outerLoopArgs;

        services::ConfigData configData;
        ControlMode pendingSelectMode{ ControlMode::torque };
        infra::Function<void(SelectResult)> pendingSelectCallback;
        uint8_t previousDefaultControlMode{ 0 };

        std::variant<std::monostate,
            application::TorqueStateMachine,
            application::SpeedStateMachine,
            application::PositionStateMachine>
            activeSm;
    };
}
