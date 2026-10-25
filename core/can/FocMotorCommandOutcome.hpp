#pragma once

#include "core/can/FocMotorCategoryServer.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/FocStateMachine.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace can
{
    inline void ReportCommandOutcome(FocMotorCategoryServer& server, uint8_t commandId, state_machine::CommandResult result,
        const infra::Function<void(services::CanAckStatus)>& onDone)
    {
        if (result == state_machine::CommandResult::ok)
            onDone(services::CanAckStatus::success);
        else if (result == state_machine::CommandResult::rejected)
            onDone(services::CanAckStatus::invalidState);
        else
            server.SendCategoryError(commandId, state_machine::ToCategoryError(result));
    }
}
