#pragma once

#include "core/foc/instantiations/FocController.hpp"
#include "core/state_machine/FocStateMachineCommon.hpp"

namespace application
{
    class TorqueStateMachine
        : public FocStateMachineCommon
    {
    public:
        TorqueStateMachine(const TerminalAndTracer& terminalAndTracer,
            const MotorHardware& hardware,
            services::NonVolatileMemory& nvm,
            const CalibrationServices& calibServices,
            state_machine::FaultNotifier& faultNotifier,
            state_machine::TransitionPolicy transitionPolicy,
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
        bool BeginQuickStop() override;
        bool ApplyProvisionalCurrentControl(const services::CalibrationData& pending) override;
        foc::CurrentLoopTunable& CurrentTunable() override;
        services::FluxLinkageIdentification* FluxLinkageIdent() override;

    private:
        foc::FocTorqueController focController;
        std::optional<services::FluxLinkageIdentificationImpl> ownFluxIdent;
        services::FluxLinkageIdentification* resolvedFluxIdent;
    };
}
