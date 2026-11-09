#pragma once

#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/ActiveDrive.hpp"
#include "core/state_machine/NvmActivity.hpp"
#include "core/state_machine/Recalibration.hpp"
#include "core/state_machine/RecalibrationStore.hpp"
#include "core/state_machine/SettingTransaction.hpp"
#include "core/state_machine/TuningSettings.hpp"
#include "infra/timer/Timer.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>
#include <optional>

namespace state_machine
{
    class ModelRecalibration
    {
    public:
        ModelRecalibration(ActiveDrive& drive, const services::ConfigData& config, services::NonVolatileMemory& nvm, application::NvmActivity& nvmActivity, SettingTransaction& transaction, TuningSettings& tunings, services::Tracer& tracer);

        SettingResult CmdRecalibrate();
        void RestoreStoredModel();
        SettingResult CmdResetModel(const infra::Function<void(bool stored)>& onStored);
        std::optional<MotorModel> ModelInUse() const;
        uint16_t RecalibrationsApplied() const;
        uint16_t RecalibrationsRejected() const;
        uint16_t RecalibrationsStored() const;

        void OnActivated();
        void RecalibrateOnReady();

    private:
        void ObserveOnlineEstimates();
        void CommissionModel();
        StoredModel StoredFor(const MotorModel& commissioned) const;
        void Conclude(const Recalibration::Outcome& outcome);
        void ApplyModelInUse();
        void Report(const Recalibration::Outcome& outcome);
        MotorModel CommissionedModel() const;
        services::RecalibrationMode RecalibrationModeSetting() const;

        ActiveDrive& drive;
        const services::ConfigData& config;
        SettingTransaction& transaction;
        TuningSettings& tunings;
        services::Tracer& tracer;
        Recalibration model;
        RecalibrationStore store;
        bool ranSinceStop{ false };
        bool loopsRecalibrated{ false };
        infra::TimerRepeating timer;
    };
}
