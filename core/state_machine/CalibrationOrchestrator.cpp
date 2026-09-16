#include "core/state_machine/CalibrationOrchestrator.hpp"
#include <bit>

namespace application
{
    CalibrationOrchestrator::CalibrationOrchestrator(
        services::ElectricalParametersIdentification& electricalIdent,
        services::MotorAlignment& motorAlignment,
        services::Tracer& tracer)
        : electricalIdent(electricalIdent)
        , motorAlignment(motorAlignment)
        , tracer(tracer)
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
        ++runToken;
        RunPolePairsStep();
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
        ++runToken;
        RunAlignmentStep();
    }

    void CalibrationOrchestrator::Abort()
    {
        aborted = true;
        electricalIdent.Abort();
        motorAlignment.Abort();
    }

    bool CalibrationOrchestrator::IsRunning() const
    {
        return electricalIdent.IsRunning();
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

                if (!angle)
                    Fail();
                else
                    onAlignmentDone(*angle);
            });
    }

    void CalibrationOrchestrator::Fail()
    {
        if (onFailed != nullptr)
            onFailed();
    }
}
