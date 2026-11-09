#include "core/state_machine/TorqueStateMachine.hpp"
#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/math/ParameterValidation.hpp"

namespace application
{
    TorqueStateMachine::TorqueStateMachine(
        services::Tracer& tracer,
        const MotorHardware& hardware,
        services::NonVolatileMemory& nvm,
        const CalibrationServices& calibServices,
        state_machine::FaultNotifier& faultNotifier,
        NvmActivity& nvmActivity,
        RotorReference rotorReference)
        : FocStateMachineCommon(tracer, hardware, nvm, calibServices, nvmActivity)
        , focController(hardware.inverter, hardware.encoder, hardware.inverter.MaxCurrentSupported())
        , resolvedFluxIdent(ResolveFluxLinkageIdent(calibServices, ownFluxIdent, focController, focController, focController, hardware.inverter))
    {
        if (hardware.currentStream != nullptr)
            focController.SetCurrentStream(*hardware.currentStream);
        RegisterFaultHandler(faultNotifier);
        Boot(rotorReference);
    }

    TorqueStateMachine::~TorqueStateMachine()
    {
        ReleaseExternalResources();
    }

    foc::FocTorque& TorqueStateMachine::GetController()
    {
        return focController;
    }

    const foc::FocTorque& TorqueStateMachine::GetController() const
    {
        return focController;
    }

    foc::SpeedLimitable& TorqueStateMachine::SpeedLimiter()
    {
        return focController;
    }

    foc::CurrentLimitable& TorqueStateMachine::CurrentLimiter()
    {
        return focController;
    }

    foc::BusVoltageTrackable& TorqueStateMachine::BusVoltageTracker()
    {
        return focController;
    }

    foc::MotionObservation TorqueStateMachine::ObserveMotion() const
    {
        return focController.ObserveMotion();
    }

    foc::FocBase& TorqueStateMachine::GetFoc()
    {
        return focController;
    }

    foc::Controllable& TorqueStateMachine::GetFocControl()
    {
        return focController;
    }

    foc::CurrentLoopTunable& TorqueStateMachine::CurrentTunable()
    {
        return focController;
    }

    services::FluxLinkageIdentification* TorqueStateMachine::FluxLinkageIdent()
    {
        return resolvedFluxIdent;
    }

    void TorqueStateMachine::ApplyModeSpecificCalibration(const services::CalibrationData& data)
    {
        const auto torqueConstant = foc::TorqueConstantFor(data.polePairs, Calibration().EffectiveFluxLinkage(data));

        if (foc::IsFinitePositive(data.coggingTorqueScale) && foc::IsFinitePositive(torqueConstant.Value()))
            focController.LoadCoggingMap(infra::MakeRange(data.coggingTorque), data.coggingTorqueScale, torqueConstant);
        else
            focController.ClearCoggingMap();
    }

    bool TorqueStateMachine::ApplyProvisionalCurrentControl(const services::CalibrationData& pending)
    {
        focController.ClearCoggingMap();
        return FocStateMachineCommon::ApplyProvisionalCurrentControl(pending);
    }

    void TorqueStateMachine::RunPostAlignmentStep(state_machine::Calibrating& calibrating)
    {
        SaveCalibration(calibrating);
    }
}
