#include "core/state_machine/CalibrationOrchestrator.hpp"
#include <bit>

namespace application
{
    CalibrationOrchestrator::CalibrationOrchestrator(
        services::ElectricalParametersIdentification& electricalIdent,
        services::MotorAlignment& motorAlignment,
        services::Tracer& tracer,
        std::optional<std::reference_wrapper<services::PhaseCurrentOffsets>> currentOffsets,
        std::optional<std::reference_wrapper<services::RotorStandstill>> standstill)
        : electricalIdent(electricalIdent)
        , motorAlignment(motorAlignment)
        , tracer(tracer)
        , currentOffsets(currentOffsets)
        , standstill(standstill)
    {}

    void CalibrationOrchestrator::Start(
        services::CalibrationData& pendingData,
        const infra::Function<void(state_machine::CalibrationStep)>& onStepChanged,
        const infra::Function<void(foc::Radians)>& onAlignmentDone,
        const infra::Function<void()>& onFailed)
    {
        this->pendingData = &pendingData;
        this->onStepChanged = onStepChanged;
        this->onAlignmentDone = onAlignmentDone;
        this->onFailed = onFailed;
        aborted = false;
        running = true;
        ++runToken;

        if (standstill)
            RunStandstillStep();
        else
            RunFirstMeasurementStep();
    }

    void CalibrationOrchestrator::StartAlignmentOnly(
        services::CalibrationData& pendingData,
        const infra::Function<void(state_machine::CalibrationStep)>& onStepChanged,
        const infra::Function<void(foc::Radians)>& onAlignmentDone,
        const infra::Function<void()>& onFailed)
    {
        this->pendingData = &pendingData;
        this->onStepChanged = onStepChanged;
        this->onAlignmentDone = onAlignmentDone;
        this->onFailed = onFailed;
        aborted = false;
        running = true;
        ++runToken;
        RunAlignmentStep();
    }

    void CalibrationOrchestrator::Abort()
    {
        aborted = true;
        running = false;
        electricalIdent.Abort();
        motorAlignment.Abort();

        if (currentOffsets)
            currentOffsets->get().Abort();

        if (standstill)
            standstill->get().Abort();
    }

    bool CalibrationOrchestrator::IsRunning() const
    {
        return running || electricalIdent.IsRunning() || (currentOffsets && currentOffsets->get().IsRunning());
    }

    bool CalibrationOrchestrator::HasRunInFlight() const
    {
        return running;
    }

    // A turning rotor drives current through windings the offset step shorts, and that current would be stored as sensor bias
    void CalibrationOrchestrator::RunStandstillStep()
    {
        tracer.Trace() << "[SM] Waiting for the rotor to come to rest";

        const auto token = runToken;
        standstill->get().Await([this, token](bool atRest)
            {
                if (aborted || token != runToken)
                    return;

                if (!atRest)
                {
                    tracer.Trace() << "[SM] The rotor did not come to rest";
                    Fail();
                }
                else
                    RunFirstMeasurementStep();
            });
    }

    void CalibrationOrchestrator::RunFirstMeasurementStep()
    {
        if (currentOffsets)
            RunCurrentOffsetsStep();
        else
            RunPolePairsStep();
    }

    // The steps after this one read the phase currents, so the offsets apply as soon as they are known.
    void CalibrationOrchestrator::RunCurrentOffsetsStep()
    {
        tracer.Trace() << "[SM] Measuring current sensor offsets";
        onStepChanged(state_machine::CalibrationStep::currentOffsets);

        const auto token = runToken;
        currentOffsets->get().Measure([this, token](std::optional<foc::PhaseCurrents> offsets)
            {
                if (aborted || token != runToken)
                    return;

                if (!offsets.has_value())
                    Fail();
                else
                {
                    pendingData->currentOffsetA = offsets->a.Value();
                    pendingData->currentOffsetB = offsets->b.Value();
                    pendingData->currentOffsetC = offsets->c.Value();
                    pendingData->currentNoiseMilliAmpere = services::CurrentNoiseMilliAmpere(currentOffsets->get().Noise().Value());
                    currentOffsets->get().Apply(*offsets);
                    RunPolePairsStep();
                }
            });
    }

    void CalibrationOrchestrator::RunPolePairsStep()
    {
        tracer.Trace() << "[SM] Identifying pole pairs";
        onStepChanged(state_machine::CalibrationStep::polePairs);

        const auto token = runToken;
        electricalIdent.EstimateNumberOfPolePairs({}, [this, token](std::optional<std::size_t> result)
            {
                if (aborted || token != runToken)
                    return;

                if (!result.has_value())
                    Fail();
                else
                {
                    pendingData->polePairs = static_cast<uint8_t>(*result);
                    RunResistanceAndInductanceStep();
                }
            });
    }

    void CalibrationOrchestrator::RunResistanceAndInductanceStep()
    {
        tracer.Trace() << "[SM] Identifying resistance and inductance";
        onStepChanged(state_machine::CalibrationStep::resistanceAndInductance);

        const auto token = runToken;
        electricalIdent.EstimateResistanceAndInductance({},
            [this, token](services::ElectricalParametersIdentification::ResistanceInductanceResult result)
            {
                if (aborted || token != runToken)
                    return;

                if (!result.resistance || !result.inductance || result.fitQuality < 0.5f)
                    Fail();
                else
                {
                    pendingData->rPhase = result.resistance->Value();
                    pendingData->lD = result.inductance->Value();
                    pendingData->lQ = result.inductance->Value();
                    RunAlignmentStep();
                }
            });
    }

    void CalibrationOrchestrator::RunAlignmentStep()
    {
        tracer.Trace() << "[SM] Aligning motor";
        onStepChanged(state_machine::CalibrationStep::alignment);

        const auto token = runToken;
        motorAlignment.ForceAlignment(pendingData->polePairs, {},
            [this, token](std::optional<foc::Radians> angle)
            {
                if (aborted || token != runToken)
                    return;

                running = false;

                if (!angle)
                    Fail();
                else
                    onAlignmentDone(*angle);
            });
    }

    void CalibrationOrchestrator::Fail()
    {
        running = false;

        if (onFailed != nullptr)
            onFailed();
    }
}
