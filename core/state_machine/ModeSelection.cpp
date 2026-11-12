#include "core/state_machine/ModeSelection.hpp"

namespace state_machine
{
    ModeSelection::ModeSelection(const ActiveDrive& drive, services::NonVolatileMemory& nvm, services::ConfigData& config, application::NvmActivity& nvmActivity, const ConfigWriter& configWriter, const infra::Function<void(ControlMode)>& activate)
        : drive(drive)
        , nvm(nvm)
        , config(config)
        , nvmActivity(nvmActivity)
        , configWriter(configWriter)
        , activate(activate)
    {}

    void ModeSelection::Select(ControlMode mode, const infra::Function<void(SelectResult)>& onDone)
    {
        if (pendingCallback != nullptr || configWriter.HasPendingPersist() || !CanSwitch())
        {
            onDone(SelectResult::busy);
            return;
        }

        previousDefaultControlMode = config.defaultControlMode;
        config.defaultControlMode = static_cast<uint8_t>(mode);
        pendingMode = mode;
        pendingCallback = onDone;
        nvmActivity.Begin();
        nvm.SaveConfig(config, [this](services::NvmStatus status)
            {
                OnSaveConfigDone(status);
            });
    }

    bool ModeSelection::IsPending() const
    {
        return pendingCallback != nullptr;
    }

    bool ModeSelection::CanSwitch() const
    {
        return IsStopped(drive.ActiveStateMachine().CurrentState()) && !drive.ActiveStateMachine().HasPendingAsyncWork();
    }

    void ModeSelection::OnSaveConfigDone(services::NvmStatus status)
    {
        nvmActivity.End();

        if (status != services::NvmStatus::Ok)
        {
            config.defaultControlMode = previousDefaultControlMode;
            pendingCallback(status == services::NvmStatus::Busy ? SelectResult::busy : SelectResult::nvmFailed);
            return;
        }

        if (CanSwitch())
        {
            activate(pendingMode);
            pendingCallback(SelectResult::ok);
            return;
        }

        config.defaultControlMode = previousDefaultControlMode;
        nvmActivity.Begin();
        nvm.SaveConfig(config, [this](services::NvmStatus)
            {
                OnRollbackSaveConfigDone();
            });
    }

    void ModeSelection::OnRollbackSaveConfigDone()
    {
        nvmActivity.End();
        pendingCallback(SelectResult::busy);
    }
}
