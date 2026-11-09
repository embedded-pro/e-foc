#include "core/state_machine/ModelRecalibration.hpp"
#include <array>

namespace state_machine
{
    ModelRecalibration::ModelRecalibration(ActiveDrive& drive, const services::ConfigData& config, services::NonVolatileMemory& nvm, application::NvmActivity& nvmActivity, SettingTransaction& transaction, TuningSettings& tunings, services::Tracer& tracer)
        : drive(drive)
        , config(config)
        , transaction(transaction)
        , tunings(tunings)
        , tracer(tracer)
        , store(nvm, nvmActivity)
        , timer(Recalibration::observationPeriod, [this]()
              {
                  ObserveOnlineEstimates();
              })
    {}

    void ModelRecalibration::OnActivated()
    {
        ranSinceStop = false;
        loopsRecalibrated = false;
    }

    SettingResult ModelRecalibration::CmdRecalibrate()
    {
        if (drive.ActiveOuterLoop() == nullptr)
            return SettingResult::wrongMode;

        if (RecalibrationModeSetting() == services::RecalibrationMode::off)
            return SettingResult::invalidState;

        if (const auto gate = transaction.Gate(); gate != SettingResult::ok)
            return gate;

        CommissionModel();
        Conclude(model.Decide(RecalibrationTrigger::command));
        ApplyModelInUse();
        return SettingResult::ok;
    }

    void ModelRecalibration::RestoreStoredModel()
    {
        store.Start([this]()
            {
                if (drive.HasMachine() && std::holds_alternative<Ready>(drive.ActiveStateMachine().CurrentState()))
                    RecalibrateOnReady();
            });
    }

    SettingResult ModelRecalibration::CmdResetModel(const infra::Function<void(bool stored)>& onStored)
    {
        if (const auto gate = transaction.Gate(); gate != SettingResult::ok)
            return gate;

        store.Reset(onStored);
        model.Revert();
        ApplyModelInUse();
        return SettingResult::ok;
    }

    uint16_t ModelRecalibration::RecalibrationsStored() const
    {
        return store.Stores();
    }

    std::optional<MotorModel> ModelRecalibration::ModelInUse() const
    {
        if (drive.ActiveOuterLoop() == nullptr)
            return std::nullopt;

        const auto commissioned = CommissionedModel();
        const auto stored = StoredFor(commissioned);
        if (model.IsCommissionedFrom(commissioned, stored))
            return model.InUse();

        Recalibration next;
        next.Commission(commissioned, stored);
        return next.InUse();
    }

    uint16_t ModelRecalibration::RecalibrationsApplied() const
    {
        return model.Applied();
    }

    uint16_t ModelRecalibration::RecalibrationsRejected() const
    {
        return model.Rejected();
    }

    void ModelRecalibration::ObserveOnlineEstimates()
    {
        if (!drive.HasMachine() || !std::holds_alternative<Enabled>(drive.ActiveStateMachine().CurrentState()))
            return;

        const auto estimates = drive.ActiveStateMachine().CurrentOnlineEstimates();
        if (!estimates.has_value())
            return;

        ranSinceStop = true;
        model.Observe(*estimates);
    }

    void ModelRecalibration::RecalibrateOnReady()
    {
        if (drive.ActiveOuterLoop() == nullptr)
            return;

        CommissionModel();
        if (ranSinceStop && RecalibrationModeSetting() == services::RecalibrationMode::automatic)
            Conclude(model.Decide(RecalibrationTrigger::automatic));

        ranSinceStop = false;
        if (RecalibrationModeSetting() != services::RecalibrationMode::off)
            store.StoreIfDue();
        ApplyModelInUse();
    }

    void ModelRecalibration::CommissionModel()
    {
        const auto commissioned = CommissionedModel();
        if (commissioned != model.Commissioned())
            loopsRecalibrated = false;

        model.Commission(commissioned, StoredFor(commissioned));
    }

    StoredModel ModelRecalibration::StoredFor(const MotorModel& commissioned) const
    {
        return RecalibrationModeSetting() != services::RecalibrationMode::off ? store.Restored(commissioned) : StoredModel{};
    }

    void ModelRecalibration::Conclude(const Recalibration::Outcome& outcome)
    {
        Report(outcome);

        const bool mechanicsOrInductance = outcome.verdicts[1] == RecalibrationVerdict::applied || outcome.verdicts[2] == RecalibrationVerdict::applied || outcome.verdicts[3] == RecalibrationVerdict::applied;
        if (mechanicsOrInductance)
            store.Store(model.Learned(), model.Commissioned());
    }

    void ModelRecalibration::ApplyModelInUse()
    {
        auto* outerLoop = drive.ActiveOuterLoop();
        const bool commissioned = model.InUse() == model.Commissioned();
        if (outerLoop == nullptr || (commissioned && !loopsRecalibrated))
            return;

        if (outerLoop->ApplyModel(model.InUse()))
        {
            loopsRecalibrated = !commissioned;
            tunings.ApplyOperatorTunings();
        }
    }

    void ModelRecalibration::Report(const Recalibration::Outcome& outcome)
    {
        constexpr std::array<const char*, Recalibration::quantities> names{ "resistance", "inductance", "inertia", "viscous friction" };
        const auto& commissioned = model.Commissioned();
        const std::array<float, Recalibration::quantities> references{ commissioned.resistance.Value(), commissioned.inductance.Value(), commissioned.inertia.Value(), commissioned.viscousFriction.Value() };

        for (std::size_t quantity = 0; quantity != Recalibration::quantities; ++quantity)
        {
            if (outcome.verdicts[quantity] == RecalibrationVerdict::applied)
                tracer.Trace() << "[SM] Recalibrated " << names[quantity] << ": " << outcome.readings[quantity] << " (commissioned " << references[quantity] << ")";
            else if (outcome.verdicts[quantity] == RecalibrationVerdict::rejected)
                tracer.Trace() << "[SM] Rejected " << names[quantity] << " reading " << outcome.readings[quantity] << ": outside its band around " << references[quantity];
        }
    }

    MotorModel ModelRecalibration::CommissionedModel() const
    {
        const auto& record = drive.CalibrationRecord();
        return MotorModel{ foc::Ohm{ record.rPhase }, foc::MilliHenry{ record.lD }, foc::NewtonMeterSecondSquared{ record.inertia }, foc::NewtonMeterSecondPerRadian{ record.frictionViscous } };
    }

    services::RecalibrationMode ModelRecalibration::RecalibrationModeSetting() const
    {
        return static_cast<services::RecalibrationMode>(config.recalibrationMode);
    }
}
