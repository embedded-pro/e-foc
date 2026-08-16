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

    void OuterLoopStateMachine::ApplyMechanics(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction, float bandwidth)
    {
        SpeedTunable().ConfigureMechanics(foc::MechanicalModelParameters{
            inertia,
            friction,
            mechTorqueConstant,
            foc::Ampere{ 0.0f },
            hal::Hertz{ 0 } });

        auto tunings = foc::SpeedLoopTunings{};
        tunings.bandwidth = bandwidth > 0.0f ? bandwidth : velocityBandwidthRadPerSec;
        SpeedTunable().SetSpeedTunings(tunings);
    }

    void OuterLoopStateMachine::ApplyModeSpecificCalibration(const services::CalibrationData& data)
    {
        ApplyMechanics(foc::NewtonMeterSecondSquared{ data.inertia }, foc::NewtonMeterSecondPerRadian{ data.frictionViscous }, data.speedLoopBandwidth);

        GetOnlineMechEstimator().SetInitialEstimate(foc::NewtonMeterSecondSquared{ data.inertia }, foc::NewtonMeterSecondPerRadian{ data.frictionViscous });
        GetOnlineElecEstimator().SetInitialEstimate(foc::Ohm{ data.rPhase }, foc::MilliHenry{ data.lD });
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
                GetTracer().Trace() << "[EST] Mech: J=" << GetOnlineMechEstimator().CurrentInertia().Value() << " B=" << GetOnlineMechEstimator().CurrentFriction().Value();
                GetTracer().Trace() << "[EST] Elec: R=" << GetOnlineElecEstimator().CurrentResistance().Value() << " L=" << GetOnlineElecEstimator().CurrentInductance().Value();
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
            ApplyElectricalModel(resistance, inductance, GetCalibration().polePairs, GetCalibration().currentLoopBandwidth);
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
        auto config = services::MechanicalParametersIdentification::Config{};

        MechIdentImpl().EstimateFrictionAndInertia(mechTorqueConstant, polePairs, config, [this](auto friction, auto inertia)
            {
                if (!IsCalibrating(state_machine::CalibrationStep::frictionAndInertia))
                    return;

                if (!friction || !inertia)
                {
                    CompletePendingCommand(state_machine::CommandResult::calibrationFailed);
                    EnterFault(state_machine::FaultCode::calibrationFailed);
                }
                else
                {
                    auto& cal = std::get<state_machine::Calibrating>(GetCurrentState());
                    cal.pendingData.inertia = inertia->Value();
                    cal.pendingData.frictionViscous = friction->Value();
                    cal.pendingData.speedLoopBandwidth = velocityBandwidthRadPerSec;
                    OnCalibrationComplete();
                }
            });
    }
}
