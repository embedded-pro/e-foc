#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryServer.hpp"
#include "core/state_machine/ControlMode.hpp"
#include "core/state_machine/ControlModeStateMachine.hpp"

namespace state_machine
{
    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    class FocMotorCanBridge
        : public services::FocMotorCategoryServerObserver
    {
        static constexpr float focCurrentScale = 100.0f;
        static constexpr float focSpeedScale = 10.0f;
        static constexpr float focPositionScale = 1000.0f;

    public:
        FocMotorCanBridge(
            services::FocMotorCategoryServer& server,
            ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>& controlMode);

        void OnQueryMotorType() override
        {}

        void OnStart() override;
        void OnStop() override;

        void OnSetPidCurrent(const services::FocPidGains&) override
        {}

        void OnSetPidSpeed(const services::FocPidGains&) override
        {}

        void OnSetPidPosition(const services::FocPidGains&) override
        {}

        void OnIdentifyElectrical() override;

        void OnIdentifyMechanical() override
        {}

        void OnRequestTelemetry() override
        {}

        void OnSetEncoderResolution(uint16_t) override
        {}

        void OnSelectControlMode(services::FocMotorMode mode) override;
        void OnSetTorqueSetpoint(int16_t value) override;
        void OnSetSpeedSetpoint(int16_t value) override;
        void OnSetPositionSetpoint(int16_t value) override;
        void OnClearFault() override;
        void OnEmergencyStop() override;

        void OnConfigureTelemetryRate(uint8_t) override
        {}

    private:
        services::FocMotorCategoryServer& server;
        ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>& controlMode;
    };

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::FocMotorCanBridge(
        services::FocMotorCategoryServer& server,
        ControlModeStateMachine<TorqueFoc, SpeedFoc, PositionFoc>& controlMode)
        : FocMotorCategoryServerObserver(server)
        , server(server)
        , controlMode(controlMode)
    {}

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::OnStart()
    {
        controlMode.ActiveStateMachine().CmdEnable();
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::OnStop()
    {
        controlMode.ActiveStateMachine().CmdDisable();
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::OnIdentifyElectrical()
    {
        controlMode.ActiveStateMachine().CmdCalibrate();
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::OnSelectControlMode(services::FocMotorMode mode)
    {
        controlMode.Select(FromCanMode(mode), [this, mode](SelectResult result)
            {
                server.SendSelectControlModeResponse(
                    ToCanMode(controlMode.Active()),
                    ToRejectReason(result));
            });
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::OnSetTorqueSetpoint(int16_t value)
    {
        const foc::IdAndIqPoint setpoint{ foc::Ampere{ 0.0f }, foc::Ampere{ static_cast<float>(value) / focCurrentScale } };
        if (!controlMode.TrySetTorque(setpoint))
            server.SendCommandRejected(services::focSetTorqueSetpointId, services::FocRejectReason::controlModeMismatch);
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::OnSetSpeedSetpoint(int16_t value)
    {
        const foc::RadiansPerSecond setpoint{ static_cast<float>(value) / focSpeedScale };
        if (!controlMode.TrySetSpeed(setpoint))
            server.SendCommandRejected(services::focSetSpeedSetpointId, services::FocRejectReason::controlModeMismatch);
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::OnSetPositionSetpoint(int16_t value)
    {
        const foc::Radians setpoint{ static_cast<float>(value) / focPositionScale };
        if (!controlMode.TrySetPosition(setpoint))
            server.SendCommandRejected(services::focSetPositionSetpointId, services::FocRejectReason::controlModeMismatch);
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::OnClearFault()
    {
        controlMode.ActiveStateMachine().CmdClearFault();
    }

    template<typename TorqueFoc, typename SpeedFoc, typename PositionFoc>
    void FocMotorCanBridge<TorqueFoc, SpeedFoc, PositionFoc>::OnEmergencyStop()
    {
        controlMode.ActiveStateMachine().CmdDisable();
    }
}
