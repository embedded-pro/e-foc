#pragma once

#include "core/foc/interfaces/Foc.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/state_machine/ActiveDrive.hpp"
#include "core/state_machine/ConfigWriter.hpp"
#include "core/state_machine/SettingTransaction.hpp"
#include "services/tracer/Tracer.hpp"
#include <optional>

namespace state_machine
{
    class TuningSettings
    {
    public:
        using OnSetting = SettingTransaction::OnSetting;

        TuningSettings(ActiveDrive& drive, services::ConfigData& config, ConfigWriter& configWriter, SettingTransaction& transaction, services::Tracer& tracer);

        TuningResult TrySetCurrentBandwidth(float bandwidth);
        TuningResult TrySetSpeedBandwidth(float bandwidth);
        TuningResult TrySetPositionBandwidth(float bandwidth);

        foc::CurrentLoopTunings StoredCurrentTunings() const;
        foc::SpeedLoopTunings StoredSpeedTunings() const;
        foc::PositionLoopTunings StoredPositionTunings() const;
        foc::CurrentLoopTunings EffectiveCurrentTunings() const;
        std::optional<foc::SpeedLoopTunings> EffectiveSpeedTunings() const;
        std::optional<foc::PositionLoopTunings> EffectivePositionTunings() const;
        void SetCurrentTunings(const foc::CurrentLoopTunings& tunings, const OnSetting& onDone);
        void SetSpeedTunings(const foc::SpeedLoopTunings& tunings, const OnSetting& onDone);
        void SetPositionTunings(const foc::PositionLoopTunings& tunings, const OnSetting& onDone);

        void SetMotorParameter(services::MotorParameter parameter, float value, const OnSetting& onDone);
        void ApplyOperatorTunings();

    private:
        TuningResult CheckRedesignPreconditions() const;

        ActiveDrive& drive;
        services::ConfigData& config;
        ConfigWriter& configWriter;
        SettingTransaction& transaction;
        services::Tracer& tracer;
    };
}
