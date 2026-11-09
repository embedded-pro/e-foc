#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/state_machine/ActiveDrive.hpp"
#include "core/state_machine/AlgorithmPersistence.hpp"
#include "core/state_machine/ModeSelection.hpp"
#include "core/state_machine/SettingTransaction.hpp"

namespace state_machine
{
    class AlgorithmSettings
    {
    public:
        using OnSetting = SettingTransaction::OnSetting;

        AlgorithmSettings(ActiveDrive& drive, AlgorithmPersistence& persistence, SettingTransaction& transaction, const ModeSelection& modes);

        foc::SelectResult SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm);
        foc::SelectResult SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm);
        foc::SelectResult SelectPositionAlgorithm(foc::PositionAlgorithm algorithm);
        void SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm, const OnSetting& onDone);
        void SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm, const OnSetting& onDone);
        void SelectPositionAlgorithm(foc::PositionAlgorithm algorithm, const OnSetting& onDone);
        foc::CurrentAlgorithm ActiveCurrentAlgorithm() const;
        foc::SpeedAlgorithm ActiveSpeedAlgorithm() const;
        foc::PositionAlgorithm ActivePositionAlgorithm() const;
        void ApplyPersistedAlgorithms();

    private:
        template<class Select>
        void SelectAndPersist(const OnSetting& onDone, Select select);

        ActiveDrive& drive;
        AlgorithmPersistence& persistence;
        SettingTransaction& transaction;
        const ModeSelection& modes;
    };
}
