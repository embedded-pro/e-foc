#pragma once

#include "core/foc/instantiations/FocController.hpp"
#include "core/state_machine/FocStateMachineCommon.hpp"

namespace application
{
    class TorqueStateMachine
        : public FocStateMachineCommon
    {
    public:
        TorqueStateMachine(services::Tracer& tracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            state_machine::FaultNotifier& faultNotifier,
            NvmActivity& nvmActivity,
            RotorReference rotorReference = RotorReference::lost);
        ~TorqueStateMachine();

        foc::FocTorque& GetController();
        const foc::FocTorque& GetController() const;
        foc::SpeedLimitable& SpeedLimiter();
        foc::CurrentLimitable& CurrentLimiter();
        foc::BusVoltageTrackable& BusVoltageTracker();

        foc::MotionObservation ObserveMotion() const override;

    protected:
        foc::FocBase& GetFoc() override;
        foc::Controllable& GetFocControl() override;
        void RunPostAlignmentStep(state_machine::Calibrating& calibrating) override;
        void ApplyModeSpecificCalibration(const services::CalibrationData& data) override;
        bool ApplyProvisionalCurrentControl(const services::CalibrationData& pending) override;
        foc::CurrentLoopTunable& CurrentTunable() override;
        services::FluxLinkageIdentification* FluxLinkageIdent() override;

    private:
        foc::FocTorqueController focController;
        std::optional<services::FluxLinkageIdentificationImpl> ownFluxIdent;
        services::FluxLinkageIdentification* resolvedFluxIdent;
    };
}
