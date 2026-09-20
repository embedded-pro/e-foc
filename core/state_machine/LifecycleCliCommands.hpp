#pragma once

#include "core/state_machine/FocStateMachine.hpp"
#include "services/util/TerminalWithStorage.hpp"

namespace application
{
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

        terminal.AddCommand({ { "align", "aln", "Re-establish rotor reference without full recalibration" },
            [getActiveSm](const infra::BoundedConstString&)
            {
                getActiveSm().CmdReAlign([](state_machine::CommandResult) {});
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
