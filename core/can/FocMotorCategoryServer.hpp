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
        virtual ~FocMotorCategoryServerObserver() = default;
        using infra::SingleObserver<FocMotorCategoryServerObserver, FocMotorCategoryServer>::SingleObserver;

        virtual void OnStart(const infra::Function<void(services::CanAckStatus)>& onDone) = 0;
        virtual void OnStop(const infra::Function<void(services::CanAckStatus)>& onDone) = 0;
        virtual void OnClearFault(const infra::Function<void(services::CanAckStatus)>& onDone) = 0;
        virtual void OnEmergencyStop(const infra::Function<void(services::CanAckStatus)>& onDone) = 0;
        virtual void OnSelectControlMode(FocMotorMode mode, const infra::Function<void(FocMotorMode)>& onActivated) = 0;
        virtual void OnSetTorqueSetpoint(foc::Ampere value, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetSpeedSetpoint(foc::RadiansPerSecond value, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPositionSetpoint(foc::Radians value, const infra::Function<void()>& onDone) = 0;

        virtual void OnSetPidCurrent(float bandwidth, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidSpeed(float bandwidth, const infra::Function<void()>& onDone) = 0;
        virtual void OnSetPidPosition(float bandwidth, const infra::Function<void()>& onDone) = 0;
        virtual void OnAlign(const infra::Function<void(services::CanAckStatus)>& onDone) = 0;
        virtual void OnIdentifyElectrical(const infra::Function<void()>& onDone) = 0;
        virtual void OnIdentifyMechanical(const infra::Function<void()>& onDone) = 0;
        virtual void OnRequestTelemetry(const infra::Function<void()>& onDone) = 0;
        virtual void OnSetEncoderResolution(uint32_t resolution, const infra::Function<void()>& onDone) = 0;
        virtual void OnConfigureTelemetryRate(uint32_t rateHz, const infra::Function<void()>& onDone) = 0;
    };

    class FocMotorCategoryServer
        : public services::CanCategoryServer
        , public infra::Subject<FocMotorCategoryServerObserver>
    {
    public:
        explicit FocMotorCategoryServer(services::CanFrameTransport& transport);
        virtual ~FocMotorCategoryServer() = default;

        uint8_t Id() const override;

        using services::CanCategoryServer::SendCategoryError;

        void SendSelectControlModeResponse(FocMotorMode activeMode);
        void SendCategoryError(uint8_t origCommandId, FocMotorCategoryError errorCode);
        void BroadcastFaultStatus(FocFaultCode fault);
        void BroadcastTelemetryStatus(FocMotorState state, FocFaultCode fault,
            foc::RadiansPerSecond speed, foc::Radians position);
        void BroadcastElectricalParams(foc::Ohm resistance, foc::MilliHenry inductance, std::size_t polePairs);
        void BroadcastMechanicalParams(foc::NewtonMeterSecondPerRadian friction, foc::NewtonMeterSecondSquared inertia);

    private:
        bool HandleStart(const hal::Can::Message& data);
        bool HandleStop(const hal::Can::Message& data);
        bool HandleClearFault(const hal::Can::Message& data);
        bool HandleEmergencyStop(const hal::Can::Message& data);
        bool HandleSelectControlMode(const hal::Can::Message& data);
        bool HandleSetTorqueSetpoint(const hal::Can::Message& data);
        bool HandleSetSpeedSetpoint(const hal::Can::Message& data);
        bool HandleSetPositionSetpoint(const hal::Can::Message& data);

        bool HandleSetPidCurrent(const hal::Can::Message& data);
        bool HandleSetPidSpeed(const hal::Can::Message& data);
        bool HandleSetPidPosition(const hal::Can::Message& data);
        bool HandleAlign(const hal::Can::Message& data);
        bool HandleIdentifyElectrical(const hal::Can::Message& data);
        bool HandleIdentifyMechanical(const hal::Can::Message& data);
        bool HandleRequestTelemetry(const hal::Can::Message& data);
        bool HandleSetEncoderResolution(const hal::Can::Message& data);
        bool HandleQueryMotorType(const hal::Can::Message& data);
        bool HandleConfigureTelemetryRate(const hal::Can::Message& data);
        bool HandleQueryContractVersion(const hal::Can::Message& data);

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
        services::CanMessageHandler<FocMotorCategoryServer> align{ focAlignId, *this, &FocMotorCategoryServer::HandleAlign };
        services::CanMessageHandler<FocMotorCategoryServer> identifyElectrical{ focIdentifyElectricalId, *this, &FocMotorCategoryServer::HandleIdentifyElectrical };
        services::CanMessageHandler<FocMotorCategoryServer> identifyMechanical{ focIdentifyMechanicalId, *this, &FocMotorCategoryServer::HandleIdentifyMechanical };
        services::CanMessageHandler<FocMotorCategoryServer> requestTelemetry{ focRequestTelemetryId, *this, &FocMotorCategoryServer::HandleRequestTelemetry };
        services::CanMessageHandler<FocMotorCategoryServer> setEncoderResolution{ focSetEncoderResolutionId, *this, &FocMotorCategoryServer::HandleSetEncoderResolution };
        services::CanMessageHandler<FocMotorCategoryServer> queryMotorType{ focQueryMotorTypeId, *this, &FocMotorCategoryServer::HandleQueryMotorType };
        services::CanMessageHandler<FocMotorCategoryServer> configureTelemetryRate{ focConfigureTelemetryRateId, *this, &FocMotorCategoryServer::HandleConfigureTelemetryRate };
        services::CanMessageHandler<FocMotorCategoryServer> queryContractVersion{ focQueryContractVersionId, *this, &FocMotorCategoryServer::HandleQueryContractVersion };
    };
}
