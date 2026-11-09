#include "core/state_machine/TuningSettings.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "core/state_machine/MotorParameterLimits.hpp"
#include "core/state_machine/OperatorTunings.hpp"

namespace state_machine
{
    TuningSettings::TuningSettings(ActiveDrive& drive, services::ConfigData& config, ConfigWriter& configWriter, SettingTransaction& transaction, services::Tracer& tracer)
        : drive(drive)
        , config(config)
        , configWriter(configWriter)
        , transaction(transaction)
        , tracer(tracer)
    {}

    TuningResult TuningSettings::CheckRedesignPreconditions() const
    {
        if (!drive.HasMachine())
            return TuningResult::wrongMode;

        if (!IsStopped(drive.ActiveStateMachine().CurrentState()))
            return TuningResult::notStopped;

        return TuningResult::ok;
    }

    TuningResult TuningSettings::TrySetCurrentBandwidth(float bandwidth)
    {
        if (!foc::IsAcceptableCurrentBandwidth(bandwidth))
            return TuningResult::outOfRange;

        if (const auto rejection = CheckRedesignPreconditions(); rejection != TuningResult::ok)
            return rejection;

        auto tunings = StoredCurrentTunings();
        tunings.bandwidth = bandwidth;

        StoreTunings(config, tunings);
        drive.Controller<foc::CurrentLoopTunable>()->SetCurrentTunings(EffectiveCurrentTunings());
        configWriter.Persist();
        return TuningResult::ok;
    }

    TuningResult TuningSettings::TrySetSpeedBandwidth(float bandwidth)
    {
        if (!foc::IsAcceptableSpeedBandwidth(bandwidth))
            return TuningResult::outOfRange;

        if (const auto rejection = CheckRedesignPreconditions(); rejection != TuningResult::ok)
            return rejection;

        auto* tunable = drive.Controller<foc::SpeedLoopTunable>();
        if (tunable == nullptr)
            return TuningResult::wrongMode;

        auto tunings = StoredSpeedTunings();
        tunings.bandwidth = bandwidth;
        if (tunable->SetSpeedTunings(tunings) != foc::SelectResult::ok)
            return TuningResult::outOfRange;

        StoreTunings(config, tunings);
        configWriter.Persist();
        return TuningResult::ok;
    }

    TuningResult TuningSettings::TrySetPositionBandwidth(float bandwidth)
    {
        if (!foc::IsAcceptablePositionBandwidth(bandwidth))
            return TuningResult::outOfRange;

        if (const auto rejection = CheckRedesignPreconditions(); rejection != TuningResult::ok)
            return rejection;

        auto* tunable = drive.Controller<foc::PositionLoopTunable>();
        if (tunable == nullptr)
            return TuningResult::wrongMode;

        auto tunings = StoredPositionTunings();
        tunings.bandwidth = bandwidth;
        if (tunable->SetPositionTunings(tunings) != foc::SelectResult::ok)
            return TuningResult::outOfRange;

        StoreTunings(config, tunings);
        configWriter.Persist();
        return TuningResult::ok;
    }

    foc::CurrentLoopTunings TuningSettings::StoredCurrentTunings() const
    {
        return state_machine::StoredCurrentTunings(config);
    }

    foc::SpeedLoopTunings TuningSettings::StoredSpeedTunings() const
    {
        return state_machine::StoredSpeedTunings(config);
    }

    foc::PositionLoopTunings TuningSettings::StoredPositionTunings() const
    {
        return state_machine::StoredPositionTunings(config);
    }

    foc::CurrentLoopTunings TuningSettings::EffectiveCurrentTunings() const
    {
        auto tunings = StoredCurrentTunings();
        if (tunings.bandwidth == 0.0f)
            tunings.bandwidth = drive.ActiveCommon().AutomaticCurrentLoopBandwidth();
        return tunings;
    }

    std::optional<foc::SpeedLoopTunings> TuningSettings::EffectiveSpeedTunings() const
    {
        const auto* outerLoop = drive.ActiveOuterLoop();
        if (outerLoop == nullptr)
            return std::nullopt;

        auto tunings = StoredSpeedTunings();
        if (tunings.bandwidth == 0.0f)
            tunings.bandwidth = outerLoop->AutomaticSpeedLoopBandwidth();
        return tunings;
    }

    std::optional<foc::PositionLoopTunings> TuningSettings::EffectivePositionTunings() const
    {
        if (drive.Active() != ControlMode::position)
            return std::nullopt;

        auto tunings = StoredPositionTunings();
        if (tunings.bandwidth == 0.0f)
            tunings.bandwidth = foc::PositionLoopTunings{}.bandwidth;
        return tunings;
    }

    void TuningSettings::ApplyOperatorTunings()
    {
        if (auto* tunable = drive.Controller<foc::CurrentLoopTunable>())
            tunable->SetCurrentTunings(EffectiveCurrentTunings());

        if (auto* tunable = drive.Controller<foc::SpeedLoopTunable>(); tunable != nullptr && tunable->SetSpeedTunings(*EffectiveSpeedTunings()) != foc::SelectResult::ok)
            tracer.Trace() << "[SM] Speed settings not applied: the active law cannot be designed with them";

        if (auto* tunable = drive.Controller<foc::PositionLoopTunable>(); tunable != nullptr && tunable->SetPositionTunings(*EffectivePositionTunings()) != foc::SelectResult::ok)
            tracer.Trace() << "[SM] Position settings not applied: the active law cannot be designed with them";
    }

    void TuningSettings::SetCurrentTunings(const foc::CurrentLoopTunings& tunings, const OnSetting& onDone)
    {
        if (!IsAcceptable(tunings))
            onDone(SettingResult::outOfRange);
        else if (!IsDesignable(tunings))
            onDone(SettingResult::invalidParameters);
        else if (const auto gate = transaction.Gate(); gate != SettingResult::ok)
            onDone(gate);
        else
        {
            StoreTunings(config, tunings);
            drive.Controller<foc::CurrentLoopTunable>()->SetCurrentTunings(EffectiveCurrentTunings());
            transaction.StoreAndPersist(onDone);
        }
    }

    void TuningSettings::SetSpeedTunings(const foc::SpeedLoopTunings& tunings, const OnSetting& onDone)
    {
        if (!IsAcceptable(tunings))
            onDone(SettingResult::outOfRange);
        else if (const auto gate = transaction.Gate(); gate != SettingResult::ok)
            onDone(gate);
        else if (drive.Controller<foc::SpeedLoopTunable>() == nullptr)
            onDone(SettingResult::wrongMode);
        else
        {
            const auto previous = StoredSpeedTunings();
            StoreTunings(config, tunings);

            if (drive.Controller<foc::SpeedLoopTunable>()->SetSpeedTunings(*EffectiveSpeedTunings()) != foc::SelectResult::ok)
            {
                StoreTunings(config, previous);
                onDone(SettingResult::invalidParameters);
                return;
            }

            transaction.StoreAndPersist(onDone);
        }
    }

    void TuningSettings::SetPositionTunings(const foc::PositionLoopTunings& tunings, const OnSetting& onDone)
    {
        if (!IsAcceptable(tunings))
            onDone(SettingResult::outOfRange);
        else if (const auto gate = transaction.Gate(); gate != SettingResult::ok)
            onDone(gate);
        else if (drive.Controller<foc::PositionLoopTunable>() == nullptr)
            onDone(SettingResult::wrongMode);
        else
        {
            const auto previous = StoredPositionTunings();
            StoreTunings(config, tunings);

            if (const auto result = drive.Controller<foc::PositionLoopTunable>()->SetPositionTunings(*EffectivePositionTunings()); result != foc::SelectResult::ok)
            {
                StoreTunings(config, previous);
                onDone(SettingTransaction::ToSettingResult(result));
                return;
            }

            transaction.StoreAndPersist(onDone);
        }
    }

    void TuningSettings::SetMotorParameter(services::MotorParameter parameter, float value, const OnSetting& onDone)
    {
        if (!IsAcceptableMotorParameter(parameter, value))
        {
            onDone(SettingResult::outOfRange);
            return;
        }

        if (const auto gate = transaction.Gate(); gate != SettingResult::ok)
        {
            onDone(gate);
            return;
        }

        transaction.Begin(onDone);
        drive.ActiveCommon().CmdSetMotorParameter(parameter, value, [this](CommandResult result)
            {
                if (result != CommandResult::ok)
                {
                    transaction.Complete(SettingTransaction::ToSettingResult(result));
                    return;
                }

                ApplyOperatorTunings();
                transaction.Complete(SettingResult::ok);
            });
    }
}
