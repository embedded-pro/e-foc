#pragma once

#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "core/state_machine/LifecycleContext.hpp"
#include "infra/util/Function.hpp"

namespace application
{
    state_machine::CommandResult ToCommandResult(services::DispatchResult result);

    class CommissioningCommands
    {
    public:
        using OnDone = infra::Function<void(state_machine::CommandResult)>;

        explicit CommissioningCommands(LifecycleMachine& stateMachine);

        void CmdReAlign(const OnDone& onDone);
        state_machine::CommandResult CmdReserveExternalCalibration();
        state_machine::CommandResult CmdReleaseExternalCalibration();
        void CmdCompleteExternalCalibration(const services::CalibrationData& data, const OnDone& onDone);
        void CmdSetMotorParameter(services::MotorParameter parameter, float value, const OnDone& onDone);

    private:
        LifecycleMachine& stateMachine;
    };
}
