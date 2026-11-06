#include "core/state_machine/OuterLoopStateMachine.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include <cmath>

namespace application
{
    OuterLoopStateMachine::OuterLoopStateMachine(
        services::Tracer& tracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices,
        foc::Ampere driveCurrentLimit,
        NvmActivity& nvmActivity)
        : FocStateMachineCommon(tracer, hardware, nvm, calibServices, nvmActivity)
        , driveCurrentLimit(driveCurrentLimit)
    {}

    foc::NewtonMeter OuterLoopStateMachine::TorqueConstantFor(const services::CalibrationData& data) const
    {
        return foc::TorqueConstantFor(data.polePairs, EffectiveFluxLinkage(data));
    }

    bool OuterLoopStateMachine::ApplyMechanics(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction, float bandwidth, foc::NewtonMeter torqueConstant)
    {
        // The cascade owns the current envelope and the outer-loop rate and substitutes them for these placeholders.
        const bool configured = SpeedTunable().ConfigureMechanics(foc::MechanicalModelParameters{
            inertia,
            friction,
            torqueConstant,
            foc::Ampere{ 0.0f },
            hal::Hertz{ 0 } });

        auto tunings = foc::SpeedLoopTunings{};
        tunings.bandwidth = foc::IsAcceptableSpeedBandwidth(bandwidth) ? bandwidth : velocityBandwidthRadPerSec;
        SpeedTunable().SetSpeedTunings(tunings);

        return configured;
    }

    float OuterLoopStateMachine::AutomaticSpeedLoopBandwidth() const
    {
        const auto bandwidth = GetCalibration().speedLoopBandwidth;
        return foc::IsAcceptableSpeedBandwidth(bandwidth) ? bandwidth : velocityBandwidthRadPerSec;
    }

    void OuterLoopStateMachine::ApplyModeSpecificCalibration(const services::CalibrationData& data)
    {
        ApplyMechanics(foc::NewtonMeterSecondSquared{ data.inertia }, foc::NewtonMeterSecondPerRadian{ data.frictionViscous }, data.speedLoopBandwidth, TorqueConstantFor(data));

        GetOnlineMechEstimator().SetInitialEstimate(foc::NewtonMeterSecondSquared{ data.inertia }, foc::NewtonMeterSecondPerRadian{ data.frictionViscous });
        GetOnlineElecEstimator().SetInitialEstimate(foc::Ohm{ data.rPhase }, foc::MilliHenry{ data.lD });
        ApplyCoggingMap(data);
    }

    void OuterLoopStateMachine::ApplyCoggingMap(const services::CalibrationData& data)
    {
        const auto torqueConstant = TorqueConstantFor(data);

        if (foc::IsFinitePositive(data.coggingTorqueScale) && foc::IsFinitePositive(torqueConstant.Value()))
            LoadCoggingCompensation(infra::MakeRange(data.coggingTorque), data.coggingTorqueScale, torqueConstant);
        else
            ClearCoggingCompensation();
    }

    bool OuterLoopStateMachine::HasValidModeSpecificCalibration(const services::CalibrationData& data) const
    {
        return foc::IsFiniteValue(data.inertia) && data.inertia > 0.0f && foc::IsFiniteValue(data.frictionViscous) && data.frictionViscous >= 0.0f;
    }

    void OuterLoopStateMachine::PrepareForEnabled()
    {
        GetOnlineMechEstimator().SetTorqueConstant(TorqueConstantFor(GetCalibration()));
    }

    std::optional<state_machine::OnlineEstimates> OuterLoopStateMachine::CurrentOnlineEstimates()
    {
        return state_machine::OnlineEstimates{
            GetOnlineElecEstimator().CurrentResistance(),
            GetOnlineElecEstimator().CurrentInductance(),
            GetOnlineMechEstimator().CurrentInertia(),
            GetOnlineMechEstimator().CurrentFriction(),
            GetOnlineElecEstimator().HasSettled(),
            GetOnlineMechEstimator().HasSettled()
        };
    }

    bool OuterLoopStateMachine::ApplyModel(const state_machine::MotorModel& model)
    {
        if (!state_machine::IsStopped(CurrentState()))
            return false;

        const auto& calibration = GetCalibration();
        ApplyMechanics(model.inertia, model.viscousFriction, calibration.speedLoopBandwidth, TorqueConstantFor(calibration));
        ApplyElectricalModel(model.resistance, model.inductance, calibration.polePairs, calibration.currentLoopBandwidth, EffectiveFluxLinkage(calibration),
            foc::Ampere{ services::CurrentNoiseAmpere(calibration.currentNoiseMilliAmpere) });
        return true;
    }

    void OuterLoopStateMachine::RunPostAlignmentStep(state_machine::Calibrating& calibrating)
    {
        RunMechanicalIdentStep(calibrating);
    }

    bool OuterLoopStateMachine::ApplyProvisionalCurrentControl(const services::CalibrationData& pending)
    {
        ClearCoggingCompensation();
        return FocStateMachineCommon::ApplyProvisionalCurrentControl(pending);
    }

    bool OuterLoopStateMachine::RunPostMechanicalStep(state_machine::Calibrating& calibrating)
    {
        auto* mapping = CoggingMappingImpl();
        if (mapping == nullptr)
            return false;

        const auto& pending = calibrating.pendingData;
        const auto torqueConstant = TorqueConstantFor(pending);

        if (!ApplyMechanics(foc::NewtonMeterSecondSquared{ pending.inertia }, foc::NewtonMeterSecondPerRadian{ pending.frictionViscous }, velocityBandwidthRadPerSec, torqueConstant))
        {
            GetTracer().Trace() << "[SM] Cogging map skipped: the measured mechanics do not configure the speed loop";
            return false;
        }

        calibrating.step = state_machine::CalibrationStep::coggingMap;
        GetTracer().Trace() << "[SM] Mapping cogging torque";

        mapping->Measure(torqueConstant, foc::NewtonMeterSecondSquared{ pending.inertia }, static_cast<std::size_t>(pending.polePairs), CoggingConfig(), [this](std::optional<services::CoggingMap> map)
            {
                Dispatch(state_machine::CoggingMapped{ map });
            });

        return true;
    }

    bool OuterLoopStateMachine::HasModeSpecificWorkPending() const
    {
        const auto* mapping = CoggingMappingImpl();
        return MechIdentImpl().IsRunning() || (mapping != nullptr && mapping->IsRunning());
    }

    void OuterLoopStateMachine::AbortModeSpecificServices()
    {
        MechIdentImpl().Abort();

        if (auto* mapping = CoggingMappingImpl())
            mapping->Abort();
    }

    services::MechanicalParametersIdentification& OuterLoopStateMachine::ResolveMechIdent(
        const CalibrationServices& calibServices,
        std::optional<infra::WithSharedAccess<services::MechanicalParametersIdentificationImpl>>& ownMechIdent,
        foc::SpeedCommandable& speedCommandable,
        foc::Controllable& drive,
        foc::PhaseCurrentsObservable& observable,
        drivers::ThreePhaseInverter& inverter,
        drivers::Encoder& encoder)
    {
        if (calibServices.mechIdentOverride.has_value())
            return calibServices.mechIdentOverride->get();

        ownMechIdent.emplace(speedCommandable, drive, observable, inverter, encoder);
        return **ownMechIdent;
    }

    services::CoggingMapping* OuterLoopStateMachine::ResolveCoggingMapping(
        const CalibrationServices& calibServices,
        std::optional<infra::WithSharedAccess<services::CoggingMappingImpl>>& ownCoggingMapping,
        foc::SpeedCommandable& speedCommandable,
        foc::Controllable& drive,
        foc::PhaseCurrentsObservable& observable,
        drivers::ThreePhaseInverter& inverter,
        drivers::Encoder& encoder)
    {
        if (calibServices.coggingMappingOverride.has_value())
            return &calibServices.coggingMappingOverride->get();

        if (!calibServices.mapCogging)
            return nullptr;

        ownCoggingMapping.emplace(speedCommandable, drive, observable, inverter, encoder);
        return &**ownCoggingMapping;
    }

    // Inert current gains or a zero speed envelope never move the rotor, so both are applied first.
    bool OuterLoopStateMachine::ApplyIdentificationControl(const services::CalibrationData& pending)
    {
        const auto torqueConstant = TorqueConstantFor(pending);

        if (!foc::IsFinitePositive(pending.rPhase) || !foc::IsFiniteValue(pending.lD) || pending.polePairs == 0 || !foc::IsFinitePositive(torqueConstant.Value()))
            return false;

        // Marked first: the electrical model sets the current tunings even when the plant is refused.
        MarkProvisionalControlApplied();

        if (!ApplyElectricalModel(foc::Ohm{ pending.rPhase }, foc::MilliHenry{ pending.lD }, pending.polePairs, pending.currentLoopBandwidth, EffectiveFluxLinkage(pending), foc::Ampere{ services::CurrentNoiseAmpere(pending.currentNoiseMilliAmpere) }))
            return false;

        return ApplyMechanics(foc::NewtonMeterSecondSquared{ provisionalInertia }, foc::NewtonMeterSecondPerRadian{ provisionalFriction }, identificationBandwidthRadPerSec, torqueConstant);
    }

    services::MechanicalParametersIdentification::Config OuterLoopStateMachine::ExcitationConfig() const
    {
        auto config = services::MechanicalParametersIdentification::Config{};
        config.maxCurrent = foc::Ampere{ driveCurrentLimit.Value() * services::identificationCurrentMargin };
        config.maxSpeed = foc::RadiansPerSecond{ config.targetSpeed.Value() * identificationSpeedMarginFactor };
        return config;
    }

    services::CoggingMapping::Config OuterLoopStateMachine::CoggingConfig() const
    {
        auto config = services::CoggingMapping::Config{};
        config.maxCurrent = foc::Ampere{ driveCurrentLimit.Value() * services::identificationCurrentMargin };
        return config;
    }

    void OuterLoopStateMachine::RunMechanicalIdentStep(state_machine::Calibrating& calibrating)
    {
        calibrating.step = state_machine::CalibrationStep::frictionAndInertia;
        const auto pending = calibrating.pendingData;
        ClearCoggingCompensation();

        if (!ApplyIdentificationControl(pending))
        {
            GetTracer().Trace() << "[SM] Mechanical identification refused: control loops would not move the rotor";
            Dispatch(state_machine::CalibrationStepFailed{});
            return;
        }

        GetTracer().Trace() << "[SM] Estimating mechanical parameters";

        MechIdentImpl().EstimateFrictionAndInertia(TorqueConstantFor(pending), static_cast<std::size_t>(pending.polePairs), ExcitationConfig(), [this](auto friction, auto inertia)
            {
                Dispatch(state_machine::MechanicalParametersIdentified{ friction, inertia, velocityBandwidthRadPerSec });
            });
    }
}
