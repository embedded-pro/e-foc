#pragma once

#include "can-lite/categories/foc_motor/FocMotorCategoryClient.hpp"
#include "can-lite/categories/foc_motor/FocMotorDefinitions.hpp"
#include "can-lite/client/CanProtocolClient.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "infra/util/Observer.hpp"
#include "tools/can_commander/adapter/CanBusAdapter.hpp"
#include <cstdint>

namespace tool
{
    class CanCommandClient;

    class CanCommandClientObserver
        : public infra::SingleObserver<CanCommandClientObserver, CanCommandClient>
    {
    public:
        using infra::SingleObserver<CanCommandClientObserver, CanCommandClient>::SingleObserver;

        virtual void OnCommandTimeout() = 0;
        virtual void OnBusyChanged(bool busy) = 0;

        virtual void OnMotorStatusReceived(services::FocMotorState state, services::FocFaultCode fault) = 0;
        virtual void OnCurrentMeasurementReceived(float idCurrent, float iqCurrent) = 0;
        virtual void OnSpeedPositionReceived(float speed, float position) = 0;
        virtual void OnBusVoltageReceived(float voltage) = 0;
        virtual void OnFaultEventReceived(services::FocFaultCode fault) = 0;

        virtual void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) = 0;

        virtual void OnConnectionChanged(bool connected) = 0;
        virtual void OnAdapterError(infra::BoundedConstString message) = 0;
    };

    class CanCommandClient
        : public infra::Subject<CanCommandClientObserver>
        , private services::CanProtocolClientObserver
        , private services::FocMotorCategoryClientObserver
        , private CanBusAdapterObserver
    {
    public:
        explicit CanCommandClient(CanBusAdapter& adapter);
        ~CanCommandClient();

        void SetNodeId(uint16_t nodeId);
        uint16_t NodeId() const;
        bool IsBusy() const;

        void SendStartMotor();
        void SendStopMotor();
        void SendEmergencyStop();
        void SendSetControlMode(services::FocMotorMode mode);
        void SendSetTorqueSetpoint(float iqCurrent);
        void SendSetSpeedSetpoint(float speedRadPerSec);
        void SendSetPositionSetpoint(float positionRad);

        void SendSetCurrentIdPid(float kp, float ki, float kd);
        void SendSetCurrentIqPid(float kp, float ki, float kd);
        void SendSetSpeedPid(float kp, float ki, float kd);
        void SendSetPositionPid(float kp, float ki, float kd);

        void SendSetSupplyVoltage(float volts) const;
        void SendSetMaxCurrent(float amps) const;

        void RequestData();

        void HandleTimeout();

    private:
        void SetBusy(bool newBusy);

        // CanProtocolClientObserver
        void OnServerOnline(uint16_t nodeId) override;
        void OnServerOffline(uint16_t nodeId) override;

        // FocMotorCategoryClientObserver
        void OnMotorTypeResponse(services::FocMotorMode mode) override;
        void OnElectricalParamsResponse(const services::FocElectricalParams& params) override;
        void OnMechanicalParamsResponse(const services::FocMechanicalParams& params) override;
        void OnTelemetryElectricalResponse(const services::FocTelemetryElectrical& telemetry) override;
        void OnTelemetryStatusResponse(const services::FocTelemetryStatus& status) override;

        // CanBusAdapterObserver
        void OnFrameLog(bool transmitted, uint32_t id, const CanFrame& data) override;
        void OnError(infra::BoundedConstString message) override;
        void OnConnectionChanged(bool connected) override;

        services::CanFrameTransport focTransport;
        services::CanProtocolClient protocolClient;
        services::FocMotorCategoryClient focCategory;
        uint16_t nodeId = 1;
        bool busy = false;
    };
}
