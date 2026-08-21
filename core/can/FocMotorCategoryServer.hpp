#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace can
{
    class FocMotorCategoryServer;

    class FocMotorCategoryServerObserver
        : public infra::SingleObserver<FocMotorCategoryServerObserver, FocMotorCategoryServer>
    {
    public:
        using infra::SingleObserver<FocMotorCategoryServerObserver, FocMotorCategoryServer>::SingleObserver;

        virtual void OnStart(const infra::Function<void(services::CanAckStatus)>& onDone) = 0;
        virtual void OnStop(const infra::Function<void(services::CanAckStatus)>& onDone) = 0;
        virtual void OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone) = 0;
        virtual void OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone) = 0;
        virtual void OnSelectControlMode(FocMotorMode mode, const infra::Function<void(FocMotorMode)>& onActivated) = 0;
        virtual void OnSetTorqueSetpoint(foc::Ampere value, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetSpeedSetpoint(foc::RadiansPerSecond value, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPositionSetpoint(foc::Radians value, const infra::Function<void()>& onDone) = 0;

        virtual void OnSetPidCurrent(const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidSpeed(const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidPosition(const infra::Function<void()>& onDone) = 0;
        virtual void OnIdentifyElectrical(const infra::Function<void()>& onDone) = 0;
        virtual void OnIdentifyMechanical(const infra::Function<void()>& onDone) = 0;
        virtual void OnRequestTelemetry(const infra::Function<void()>& onDone) = 0;
        virtual void OnSetEncoderResolution(const infra::Function<void()>& onDone) = 0;
        virtual void OnConfigureTelemetryRate(const infra::Function<void()>& onDone) = 0;
    };

    class FocMotorCategoryServer
        : public services::CanCategoryServer
        , public infra::Subject<FocMotorCategoryServerObserver>
    {
    public:
        explicit FocMotorCategoryServer(services::CanFrameTransport& transport);

        uint8_t Id() const override;

        using services::CanCategoryServer::SendCategoryError;

        void SendSelectControlModeResponse(FocMotorMode activeMode);
        void SendCategoryError(uint8_t origCommandId, FocMotorCategoryError errorCode);
        void BroadcastFaultStatus(FocFaultCode fault);

    private:
        void HandleStart(const hal::Can::Message& data);
        void HandleStop(const hal::Can::Message& data);
        void HandleClearFault(const hal::Can::Message& data);
        void HandleEmergencyStop(const hal::Can::Message& data);
        void HandleSelectControlMode(const hal::Can::Message& data);
        void HandleSetTorqueSetpoint(const hal::Can::Message& data);
        void HandleSetSpeedSetpoint(const hal::Can::Message& data);
        void HandleSetPositionSetpoint(const hal::Can::Message& data);

        void HandleSetPidCurrent(const hal::Can::Message& data);
        void HandleSetPidSpeed(const hal::Can::Message& data);
        void HandleSetPidPosition(const hal::Can::Message& data);
        void HandleIdentifyElectrical(const hal::Can::Message& data);
        void HandleIdentifyMechanical(const hal::Can::Message& data);
        void HandleRequestTelemetry(const hal::Can::Message& data);
        void HandleSetEncoderResolution(const hal::Can::Message& data);
        void HandleQueryMotorType(const hal::Can::Message& data);
        void HandleConfigureTelemetryRate(const hal::Can::Message& data);

        services::CanMessageHandler<FocMotorCategoryServer> start{ focStartId, *this, &FocMotorCategoryServer::HandleStart };
        services::CanMessageHandler<FocMotorCategoryServer> stop{ focStopId, *this, &FocMotorCategoryServer::HandleStop };
        services::CanMessageHandler<FocMotorCategoryServer> clearFault{ focClearFaultId, *this, &FocMotorCategoryServer::HandleClearFault };
        services::CanMessageHandler<FocMotorCategoryServer> emergencyStop{ focEmergencyStopId, *this, &FocMotorCategoryServer::HandleEmergencyStop };
        services::CanMessageHandler<FocMotorCategoryServer> selectControlMode{ focSelectControlModeId, *this, &FocMotorCategoryServer::HandleSelectControlMode };
        services::CanMessageHandler<FocMotorCategoryServer> setTorqueSetpoint{ focSetTorqueSetpointId, *this, &FocMotorCategoryServer::HandleSetTorqueSetpoint };
        services::CanMessageHandler<FocMotorCategoryServer> setSpeedSetpoint{ focSetSpeedSetpointId, *this, &FocMotorCategoryServer::HandleSetSpeedSetpoint };
        services::CanMessageHandler<FocMotorCategoryServer> setPositionSetpoint{ focSetPositionSetpointId, *this, &FocMotorCategoryServer::HandleSetPositionSetpoint };
        services::CanMessageHandler<FocMotorCategoryServer> setPidCurrent{ focSetPidCurrentId, *this, &FocMotorCategoryServer::HandleSetPidCurrent };
        services::CanMessageHandler<FocMotorCategoryServer> setPidSpeed{ focSetPidSpeedId, *this, &FocMotorCategoryServer::HandleSetPidSpeed };
        services::CanMessageHandler<FocMotorCategoryServer> setPidPosition{ focSetPidPositionId, *this, &FocMotorCategoryServer::HandleSetPidPosition };
        services::CanMessageHandler<FocMotorCategoryServer> identifyElectrical{ focIdentifyElectricalId, *this, &FocMotorCategoryServer::HandleIdentifyElectrical };
        services::CanMessageHandler<FocMotorCategoryServer> identifyMechanical{ focIdentifyMechanicalId, *this, &FocMotorCategoryServer::HandleIdentifyMechanical };
        services::CanMessageHandler<FocMotorCategoryServer> requestTelemetry{ focRequestTelemetryId, *this, &FocMotorCategoryServer::HandleRequestTelemetry };
        services::CanMessageHandler<FocMotorCategoryServer> setEncoderResolution{ focSetEncoderResolutionId, *this, &FocMotorCategoryServer::HandleSetEncoderResolution };
        services::CanMessageHandler<FocMotorCategoryServer> queryMotorType{ focQueryMotorTypeId, *this, &FocMotorCategoryServer::HandleQueryMotorType };
        services::CanMessageHandler<FocMotorCategoryServer> configureTelemetryRate{ focConfigureTelemetryRateId, *this, &FocMotorCategoryServer::HandleConfigureTelemetryRate };
    };
}
