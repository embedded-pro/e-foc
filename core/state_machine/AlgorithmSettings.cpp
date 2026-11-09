#include "core/state_machine/AlgorithmSettings.hpp"

namespace state_machine
{
    AlgorithmSettings::AlgorithmSettings(ActiveDrive& drive, AlgorithmPersistence& persistence, SettingTransaction& transaction, const ModeSelection& modes)
        : drive(drive)
        , persistence(persistence)
        , transaction(transaction)
        , modes(modes)
    {}

    foc::SelectResult AlgorithmSettings::SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm)
    {
        if (!modes.CanSwitch())
            return foc::SelectResult::busy;
        return persistence.SelectCurrentAlgorithm(algorithm, drive.Controller<foc::CurrentLoopSelectable>());
    }

    foc::SelectResult AlgorithmSettings::SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm)
    {
        if (!modes.CanSwitch())
            return foc::SelectResult::busy;
        return persistence.SelectSpeedAlgorithm(algorithm, drive.Controller<foc::SpeedLoopSelectable>());
    }

    foc::SelectResult AlgorithmSettings::SelectPositionAlgorithm(foc::PositionAlgorithm algorithm)
    {
        if (!modes.CanSwitch())
            return foc::SelectResult::busy;
        return persistence.SelectPositionAlgorithm(algorithm, drive.Controller<foc::PositionLoopSelectable>());
    }

    template<class Select>
    void AlgorithmSettings::SelectAndPersist(const OnSetting& onDone, Select select)
    {
        if (const auto gate = transaction.Gate(); gate != SettingResult::ok)
        {
            onDone(gate);
            return;
        }

        transaction.Begin(onDone);
        const auto result = select([this](services::NvmStatus status)
            {
                transaction.CompleteWith(status);
            });

        if (result != foc::SelectResult::ok)
            transaction.Complete(SettingTransaction::ToSettingResult(result));
    }

    void AlgorithmSettings::SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm, const OnSetting& onDone)
    {
        SelectAndPersist(onDone, [this, algorithm](const AlgorithmPersistence::OnPersisted& onPersisted)
            {
                return persistence.SelectCurrentAlgorithm(algorithm, drive.Controller<foc::CurrentLoopSelectable>(), onPersisted);
            });
    }

    void AlgorithmSettings::SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm, const OnSetting& onDone)
    {
        SelectAndPersist(onDone, [this, algorithm](const AlgorithmPersistence::OnPersisted& onPersisted)
            {
                return persistence.SelectSpeedAlgorithm(algorithm, drive.Controller<foc::SpeedLoopSelectable>(), onPersisted);
            });
    }

    void AlgorithmSettings::SelectPositionAlgorithm(foc::PositionAlgorithm algorithm, const OnSetting& onDone)
    {
        SelectAndPersist(onDone, [this, algorithm](const AlgorithmPersistence::OnPersisted& onPersisted)
            {
                return persistence.SelectPositionAlgorithm(algorithm, drive.Controller<foc::PositionLoopSelectable>(), onPersisted);
            });
    }

    foc::CurrentAlgorithm AlgorithmSettings::ActiveCurrentAlgorithm() const
    {
        return persistence.ActiveCurrentAlgorithm(std::as_const(drive).Controller<foc::CurrentLoopSelectable>());
    }

    foc::SpeedAlgorithm AlgorithmSettings::ActiveSpeedAlgorithm() const
    {
        return persistence.ActiveSpeedAlgorithm(std::as_const(drive).Controller<foc::SpeedLoopSelectable>());
    }

    foc::PositionAlgorithm AlgorithmSettings::ActivePositionAlgorithm() const
    {
        return persistence.ActivePositionAlgorithm(std::as_const(drive).Controller<foc::PositionLoopSelectable>());
    }

    void AlgorithmSettings::ApplyPersistedAlgorithms()
    {
        persistence.ApplyPersistedAlgorithms(drive.Controller<foc::CurrentLoopSelectable>(), drive.Controller<foc::SpeedLoopSelectable>(), drive.Controller<foc::PositionLoopSelectable>());
    }
}
