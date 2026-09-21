#include "core/state_machine/OuterLoopStateMachine.hpp"
#include "core/foc/math/FiniteGuard.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include <cmath>

namespace application
{
    OuterLoopStateMachine::OuterLoopStateMachine(
        const TerminalAndTracer& terminalAndTracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices,
        foc::Ampere driveCurrentLimit)
        : FocStateMachineCommon(terminalAndTracer, hardware, nvm, calibServices)
        , mechTorqueConstant(calibServices.mechTorqueConstant)
        , driveCurrentLimit(driveCurrentLimit)
    {}

    bool OuterLoopStateMachine::ApplyMechanics(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction, float bandwidth)
    {
        // The cascade owns the current envelope and the outer-loop rate and substitutes them for these placeholders.
        const bool configured = SpeedTunable().ConfigureMechanics(foc::MechanicalModelParameters{
            inertia,
            friction,
            mechTorqueConstant,
            foc::Ampere{ 0.0f },
            hal::Hertz{ 0 } });

        auto tunings = foc::SpeedLoopTunings{};
        tunings.bandwidth = foc::IsAcceptableSpeedBandwidth(bandwidth) ? bandwidth : velocityBandwidthRadPerSec;
        SpeedTunable().SetSpeedTunings(tunings);

        return configured;
    }

    void OuterLoopStateMachine::ApplyModeSpecificCalibration(const services::CalibrationData& data)
    {
        ApplyMechanics(foc::NewtonMeterSecondSquared{ data.inertia }, foc::NewtonMeterSecondPerRadian{ data.frictionViscous }, data.speedLoopBandwidth);

        GetOnlineMechEstimator().SetInitialEstimate(foc::NewtonMeterSecondSquared{ data.inertia }, foc::NewtonMeterSecondPerRadian{ data.frictionViscous });
        GetOnlineElecEstimator().SetInitialEstimate(foc::Ohm{ data.rPhase }, foc::MilliHenry{ data.lD });
    }

    bool OuterLoopStateMachine::HasValidModeSpecificCalibration(const services::CalibrationData& data) const
    {
        return foc::IsFiniteValue(data.inertia) && data.inertia > 0.0f && foc::IsFiniteValue(data.frictionViscous) && data.frictionViscous >= 0.0f;
    }

    void OuterLoopStateMachine::PrepareForEnabled()
    {
        GetOnlineMechEstimator().SetTorqueConstant(mechTorqueConstant);
    }

    void OuterLoopStateMachine::RegisterModeSpecificCli(services::TerminalWithStorage& terminal)
    {
        terminal.AddCommand({ { "estimate_status", "es", "Print current online estimates" },
            [this](const infra::BoundedConstString&)
            {
                TraceOnlineEstimates();
            } });
    }

    void OuterLoopStateMachine::TraceOnlineEstimates()
    {
        GetTracer().Trace() << "[EST] Mech: J=" << GetOnlineMechEstimator().CurrentInertia().Value() << " B=" << GetOnlineMechEstimator().CurrentFriction().Value();
        GetTracer().Trace() << "[EST] Elec: R=" << GetOnlineElecEstimator().CurrentResistance().Value() << " L=" << GetOnlineElecEstimator().CurrentInductance().Value();
    }

    void OuterLoopStateMachine::ApplyOnlineEstimates()
    {
        if (!std::holds_alternative<state_machine::Enabled>(CurrentState()))
            return;

        const auto inertia = GetOnlineMechEstimator().CurrentInertia();
        const auto friction = GetOnlineMechEstimator().CurrentFriction();

        if (!std::isfinite(inertia.Value()) || inertia.Value() <= 0.0f ||
            !std::isfinite(friction.Value()) || friction.Value() <= 0.0f)
            GetTracer().Trace() << "[SM] Skipping mechanical estimates: non-physical values (J=" << inertia.Value() << " B=" << friction.Value() << ")";
        else
        {
            GetTracer().Trace() << "[SM] Applying mechanical estimates: J=" << inertia.Value() << " B=" << friction.Value();
            ApplyMechanics(inertia, friction, velocityBandwidthRadPerSec);
        }

        const auto resistance = GetOnlineElecEstimator().CurrentResistance();
        const auto inductance = GetOnlineElecEstimator().CurrentInductance();
        if (!std::isfinite(resistance.Value()) || resistance.Value() <= 0.0f ||
            !std::isfinite(inductance.Value()) || inductance.Value() <= 0.0f)
            GetTracer().Trace() << "[SM] Skipping electrical estimates: non-physical values (R=" << resistance.Value() << " L=" << inductance.Value() << ")";
        else
        {
            GetTracer().Trace() << "[SM] Applying electrical estimates: R=" << resistance.Value() << " L=" << inductance.Value();
            ApplyElectricalModel(resistance, inductance, GetCalibration().polePairs, GetCalibration().currentLoopBandwidth, EffectiveFluxLinkage(GetCalibration()));
        }
    }

    void OuterLoopStateMachine::RunPostAlignmentStep(state_machine::Calibrating& calibrating)
    {
        RunMechanicalIdentStep(calibrating);
    }

    bool OuterLoopStateMachine::HasModeSpecificWorkPending() const
    {
        return MechIdentImpl().IsRunning();
    }

    void OuterLoopStateMachine::AbortModeSpecificServices()
    {
        MechIdentImpl().Abort();
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

    // Identification needs the loops it is about to excite to be live. Until the electrical model measured
    // moments ago is applied the current loop holds inert gains, and until mechanics are configured the speed
    // loop holds a zero current envelope, so the commanded trajectory never reaches the rotor.
    bool OuterLoopStateMachine::ApplyIdentificationControl(const services::CalibrationData& pending)
    {
        if (!foc::IsFinitePositive(pending.rPhase) || !foc::IsFiniteValue(pending.lD) || pending.polePairs == 0 || !foc::IsFinitePositive(mechTorqueConstant.Value()))
            return false;

        // Marked before the first call, not after: applying an electrical model sets the current-loop
        // tunings whether or not the plant itself was accepted, so from here on there is something to
        // restore even when this returns false.
        MarkProvisionalControlApplied();

        if (!ApplyElectricalModel(foc::Ohm{ pending.rPhase }, foc::MilliHenry{ pending.lD }, pending.polePairs, pending.currentLoopBandwidth, EffectiveFluxLinkage(pending)))
            return false;

        return ApplyMechanics(foc::NewtonMeterSecondSquared{ provisionalInertia }, foc::NewtonMeterSecondPerRadian{ provisionalFriction }, identificationBandwidthRadPerSec);
    }

    services::MechanicalParametersIdentification::Config OuterLoopStateMachine::ExcitationConfig() const
    {
        auto config = services::MechanicalParametersIdentification::Config{};
        config.maxCurrent = foc::Ampere{ driveCurrentLimit.Value() * identificationCurrentMarginFactor };
        config.maxSpeed = foc::RadiansPerSecond{ config.targetSpeed.Value() * identificationSpeedMarginFactor };
        return config;
    }

    void OuterLoopStateMachine::RunMechanicalIdentStep(state_machine::Calibrating& calibrating)
    {
        calibrating.step = state_machine::CalibrationStep::frictionAndInertia;
        const auto pending = calibrating.pendingData;

        if (!ApplyIdentificationControl(pending))
        {
            GetTracer().Trace() << "[SM] Mechanical identification refused: control loops would not move the rotor";
            Dispatch(state_machine::CalibrationStepFailed{});
            return;
        }

        GetTracer().Trace() << "[SM] Estimating mechanical parameters";

        MechIdentImpl().EstimateFrictionAndInertia(mechTorqueConstant, static_cast<std::size_t>(pending.polePairs), ExcitationConfig(), [this](auto friction, auto inertia)
            {
                Dispatch(state_machine::MechanicalParametersIdentified{ friction, inertia, velocityBandwidthRadPerSec });
            });
    }
}
