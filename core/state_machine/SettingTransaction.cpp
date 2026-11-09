#include "core/state_machine/SettingTransaction.hpp"

namespace state_machine
{
    SettingTransaction::SettingTransaction(const ActiveDrive& drive, ConfigWriter& configWriter, const ModeSelection& modes)
        : drive(drive)
        , configWriter(configWriter)
        , modes(modes)
    {}

    SettingResult SettingTransaction::Gate() const
    {
        if (!IsStopped(drive.ActiveStateMachine().CurrentState()))
            return SettingResult::invalidState;

        if (pending != nullptr || modes.IsPending() || drive.ActiveStateMachine().HasPendingAsyncWork() || configWriter.HasPendingPersist())
            return SettingResult::busy;

        return SettingResult::ok;
    }

    bool SettingTransaction::IsIdle() const
    {
        return Gate() == SettingResult::ok;
    }

    void SettingTransaction::Begin(const OnSetting& onDone)
    {
        pending = onDone;
    }

    void SettingTransaction::StoreAndPersist(const OnSetting& onDone)
    {
        Begin(onDone);
        configWriter.Persist([this](services::NvmStatus status)
            {
                CompleteWith(status);
            });
    }

    void SettingTransaction::Complete(SettingResult result)
    {
        auto onDone = pending;
        pending = nullptr;
        onDone(result);
    }

    void SettingTransaction::CompleteWith(services::NvmStatus status)
    {
        Complete(status == services::NvmStatus::Ok ? SettingResult::ok : SettingResult::nvmFailed);
    }

    SettingResult SettingTransaction::ToSettingResult(CommandResult result)
    {
        switch (result)
        {
            case CommandResult::nvmFailed:
                return SettingResult::nvmFailed;
            case CommandResult::abortedByFault:
                return SettingResult::aborted;
            default:
                return SettingResult::busy;
        }
    }

    SettingResult SettingTransaction::ToSettingResult(foc::SelectResult result)
    {
        switch (result)
        {
            case foc::SelectResult::ok:
                return SettingResult::ok;
            case foc::SelectResult::busy:
                return SettingResult::busy;
            case foc::SelectResult::invalidAlgorithm:
                return SettingResult::wrongMode;
            default:
                return SettingResult::invalidParameters;
        }
    }
}
