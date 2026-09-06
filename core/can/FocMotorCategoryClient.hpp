#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceSource.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace can
{
    class FocMotorCategoryClient;

    class FocMotorCategoryClientObserver
        : public infra::SingleObserver<FocMotorCategoryClientObserver, FocMotorCategoryClient>
    {
    public:
        virtual ~FocMotorCategoryClientObserver() = default;
        using infra::SingleObserver<FocMotorCategoryClientObserver, FocMotorCategoryClient>::SingleObserver;

        virtual void OnSelectControlModeResponse(FocMotorMode activeMode) = 0;
        virtual void OnCategoryError(uint8_t originCommandId, FocMotorCategoryError errorCode) = 0;
        virtual void OnTelemetryStatus(const hal::Can::Message& msg) = 0;
        virtual void OnTelemetryElectrical(const hal::Can::Message& msg) = 0;
    };

    class FocMotorCategoryClient
        : public services::CanCategoryClient
        , public infra::Subject<FocMotorCategoryClientObserver>
    {
    public:
        FocMotorCategoryClient(services::CanFrameTransport& transport, services::CanSequenceSource& sequenceSource);

        uint8_t Id() const override;

        bool SendStart(uint16_t targetNodeId);
        bool SendStop(uint16_t targetNodeId);
        bool SendClearFault(uint16_t targetNodeId);
        bool SendEmergencyStop(uint16_t targetNodeId);
        bool SendSelectControlMode(uint16_t targetNodeId, FocMotorMode mode);
        bool SendSetTorqueSetpoint(uint16_t targetNodeId, foc::Ampere value);
        bool SendSetSpeedSetpoint(uint16_t targetNodeId, foc::RadiansPerSecond value);
        bool SendSetPositionSetpoint(uint16_t targetNodeId, foc::Radians value);

        bool SendSetCurrentIdPid(uint16_t targetNodeId, float kp, float ki, float kd);
        bool SendSetCurrentIqPid(uint16_t targetNodeId, float kp, float ki, float kd);
        bool SendSetSpeedPid(uint16_t targetNodeId, float kp, float ki, float kd);
        bool SendSetPositionPid(uint16_t targetNodeId, float kp, float ki, float kd);

    private:
        bool SendSetCurrentPid(uint16_t targetNodeId, uint8_t axis, float kp, float ki, float kd);

        void HandleSelectControlModeResponse(const hal::Can::Message& data);
        void HandleCategoryError(const hal::Can::Message& data);
        void HandleTelemetryStatus(const hal::Can::Message& data);
        void HandleTelemetryElectrical(const hal::Can::Message& data);

        services::CanMessageHandler<FocMotorCategoryClient> selectControlModeResponse{ focSelectControlModeResponseId, *this, &FocMotorCategoryClient::HandleSelectControlModeResponse };
        services::CanMessageHandler<FocMotorCategoryClient> categoryError{ services::canCategoryErrorResponseMessageTypeId, *this, &FocMotorCategoryClient::HandleCategoryError };
        services::CanMessageHandler<FocMotorCategoryClient> telemetryStatus{ focTelemetryStatusResponseId, *this, &FocMotorCategoryClient::HandleTelemetryStatus };
        services::CanMessageHandler<FocMotorCategoryClient> telemetryElectrical{ focTelemetryElectricalResponseId, *this, &FocMotorCategoryClient::HandleTelemetryElectrical };
    };
}
