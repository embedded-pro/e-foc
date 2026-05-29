#include "core/state_machine/OuterLoopStateMachine.hpp"
#include <cmath>

namespace application
{
    OuterLoopStateMachine::OuterLoopStateMachine(
        const TerminalAndTracer& terminalAndTracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        services::ElectricalParametersIdentification& electricalIdent,
        services::MotorAlignment& motorAlignment,
        foc::NewtonMeter mechTorqueConstantArg)
        : FocStateMachineCommon(terminalAndTracer, hardware, nvm, electricalIdent, motorAlignment)
        , mechTorqueConstant(mechTorqueConstantArg)
    {}

    void OuterLoopStateMachine::ApplyModeSpecificCalibration(const services::CalibrationData& data)
    {
        SpeedTunable().SetSpeedTunings(GetVdc(), foc::SpeedTunings{ data.kpVelocity, data.kiVelocity, 0.0f });

        if (data.inertia > 0.0f)
        {
            GetOnlineMechEstimator().SetInitialEstimate(
                foc::NewtonMeterSecondSquared{ data.inertia },
                foc::NewtonMeterSecondPerRadian{ data.frictionViscous });
        }
        GetOnlineElecEstimator().SetInitialEstimate(
            foc::Ohm{ data.rPhase },
            foc::MilliHenry{ data.lD });
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
                GetTracer().Trace() << "[EST] Mech: J=" << GetOnlineMechEstimator().CurrentInertia().Value()
                                    << " B=" << GetOnlineMechEstimator().CurrentFriction().Value();
                GetTracer().Trace() << "[EST] Elec: R=" << GetOnlineElecEstimator().CurrentResistance().Value()
                                    << " L=" << GetOnlineElecEstimator().CurrentInductance().Value();
            } });
    }

    void OuterLoopStateMachine::ApplyOnlineEstimates()
    {
        if (!std::holds_alternative<state_machine::Enabled>(GetCurrentState()))
            return;

        const auto inertia = GetOnlineMechEstimator().CurrentInertia();
        const auto friction = GetOnlineMechEstimator().CurrentFriction();
        if (!std::isfinite(inertia.Value()) || inertia.Value() <= 0.0f ||
            !std::isfinite(friction.Value()) || friction.Value() <= 0.0f)
        {
            GetTracer().Trace() << "[SM] Skipping mechanical estimates: non-physical values (J="
                                << inertia.Value() << " B=" << friction.Value() << ")";
        }
        else
        {
            GetTracer().Trace() << "[SM] Applying mechanical estimates: J="
                                << inertia.Value() << " B=" << friction.Value();
            GetSpeedAutoTuner().SetPidBasedOnInertiaAndFriction(
                GetVdc(), inertia, friction, velocityBandwidthRadPerSec);
        }

        const auto resistance = GetOnlineElecEstimator().CurrentResistance();
        const auto inductance = GetOnlineElecEstimator().CurrentInductance();
        if (!std::isfinite(resistance.Value()) || resistance.Value() <= 0.0f ||
            !std::isfinite(inductance.Value()) || inductance.Value() <= 0.0f)
        {
            GetTracer().Trace() << "[SM] Skipping electrical estimates: non-physical values (R="
                                << resistance.Value() << " L=" << inductance.Value() << ")";
        }
        else
        {
            GetTracer().Trace() << "[SM] Applying electrical estimates: R="
                                << resistance.Value() << " L=" << inductance.Value();
            GetCurrentLoopTuner().SetPidBasedOnResistanceAndInductance(
                GetVdc(), resistance, inductance, GetInverter().BaseFrequency(), nyquistFactor);
        }
    }

    void OuterLoopStateMachine::RunPostAlignmentStep()
    {
        RunMechanicalIdentStep();
    }

    void OuterLoopStateMachine::RunMechanicalIdentStep()
    {
        GetTracer().Trace() << "[SM] Estimating mechanical parameters";

        auto& calibrating = std::get<state_machine::Calibrating>(GetCurrentState());
        calibrating.step = state_machine::CalibrationStep::frictionAndInertia;
        const auto polePairs = static_cast<std::size_t>(calibrating.pendingData.polePairs);

        MechIdentImpl().EstimateFrictionAndInertia(
            mechTorqueConstant,
            polePairs,
            services::MechanicalParametersIdentification::Config{},
            [this](std::optional<foc::NewtonMeterSecondPerRadian> friction,
                std::optional<foc::NewtonMeterSecondSquared> inertia)
            {
                if (!IsCalibrating(state_machine::CalibrationStep::frictionAndInertia))
                    return;
                if (!friction || !inertia)
                {
                    CompletePendingCommand(state_machine::CommandResult::calibrationFailed);
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                    return;
                }
                auto& cal = std::get<state_machine::Calibrating>(GetCurrentState());
                cal.pendingData.inertia = inertia->Value();
                cal.pendingData.frictionViscous = friction->Value();
                cal.pendingData.kpVelocity = inertia->Value() * velocityBandwidthRadPerSec;
                cal.pendingData.kiVelocity = friction->Value() * velocityBandwidthRadPerSec;
                OnCalibrationComplete();
            });
    }
}
