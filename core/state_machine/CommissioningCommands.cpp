#include "core/state_machine/CommissioningCommands.hpp"

namespace application
{
    state_machine::CommandResult ToCommandResult(services::DispatchResult result)
    {
        if (result == services::DispatchResult::rejected || result == services::DispatchResult::forbidden)
            return state_machine::CommandResult::rejected;

        if (result == services::DispatchResult::queued)
            return state_machine::CommandResult::queued;

        return state_machine::CommandResult::ok;
    }

    CommissioningCommands::CommissioningCommands(LifecycleMachine& stateMachine)
        : stateMachine(stateMachine)
    {}

    void CommissioningCommands::CmdReAlign(const OnDone& onDone)
    {
        stateMachine.Dispatch(state_machine::ReAlign{ state_machine::CommandCallback{ onDone } });
    }

    state_machine::CommandResult CommissioningCommands::CmdReserveExternalCalibration()
    {
        return ToCommandResult(stateMachine.Dispatch(state_machine::ReserveExternalCalibration{}));
    }

    state_machine::CommandResult CommissioningCommands::CmdReleaseExternalCalibration()
    {
        return ToCommandResult(stateMachine.Dispatch(state_machine::ReleaseExternalCalibration{}));
    }

    void CommissioningCommands::CmdCompleteExternalCalibration(const services::CalibrationData& data, const OnDone& onDone)
    {
        stateMachine.Dispatch(state_machine::CompleteExternalCalibration{ data, state_machine::CommandCallback{ onDone } });
    }

    void CommissioningCommands::CmdSetMotorParameter(services::MotorParameter parameter, float value, const OnDone& onDone)
    {
        stateMachine.Dispatch(state_machine::SetMotorParameter{ parameter, value, state_machine::CommandCallback{ onDone } });
    }
}
