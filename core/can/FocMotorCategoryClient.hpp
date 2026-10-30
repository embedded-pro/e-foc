#pragma once

#include "can-lite/core/CanCategory.hpp"
#include "can-lite/core/CanFrameTransport.hpp"
#include "can-lite/core/CanMessageHandler.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "can-lite/core/CanSequenceSource.hpp"
#include "core/can/CanFrameOrigin.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <array>
#include <cstdint>

namespace can
{
    class FocMotorCategoryClient;

    class FocMotorCategoryClientObserver
        : public infra::SingleObserver<FocMotorCategoryClientObserver, FocMotorCategoryClient>
    {
    protected:
        ~FocMotorCategoryClientObserver() = default;

    public:
        using infra::SingleObserver<FocMotorCategoryClientObserver, FocMotorCategoryClient>::SingleObserver;

        virtual void OnSelectControlModeResponse(uint16_t sourceNodeId, FocMotorMode activeMode) = 0;
        virtual void OnCategoryError(uint16_t sourceNodeId, uint8_t originCommandId, FocMotorCategoryError errorCode) = 0;
        virtual void OnTelemetryStatus(uint16_t sourceNodeId, const hal::Can::Message& msg) = 0;
        virtual void OnTelemetryElectrical(uint16_t sourceNodeId, const hal::Can::Message& msg) = 0;
        virtual void OnContractVersionResponse(uint16_t sourceNodeId, uint8_t major, uint8_t minor) = 0;
        virtual void OnElectricalParamsResponse(uint16_t sourceNodeId, foc::Ohm resistance, foc::MilliHenry inductance, uint8_t polePairs) = 0;
        virtual void OnAlgorithmsResponse(uint16_t sourceNodeId, const FocAlgorithms& algorithms) = 0;
        virtual void OnParameterResponse(uint16_t sourceNodeId, FocParameter parameter, FocParameterValue value) = 0;
    };

    enum class ContractCompatibility : uint8_t
    {
        unknown,
        compatible,
        incompatible
    };

    class NodeContracts
    {
    public:
        static constexpr std::size_t maxTracked{ 8 };

        ContractCompatibility Of(uint16_t nodeId) const;
        bool Incompatible(uint16_t nodeId) const;
        void Record(uint16_t nodeId, ContractCompatibility compatibility);

    private:
        struct NodeContract
        {
            uint16_t nodeId{ 0 };
            ContractCompatibility compatibility{ ContractCompatibility::unknown };
        };

        std::array<NodeContract, maxTracked> contracts{};
        std::size_t nextEvicted{ 0 };
    };

    class FocMotorCategoryClient
        : public services::CanCategoryClient
        , public infra::Subject<FocMotorCategoryClientObserver>
    {
    public:
        static constexpr std::size_t maxTrackedNodes{ NodeContracts::maxTracked };

        FocMotorCategoryClient(services::CanFrameTransport& transport, services::CanSequenceSource& sequenceSource, const CanFrameOrigin& origin);

        uint8_t Id() const override;

        bool SendStart(uint16_t targetNodeId);
        bool SendStop(uint16_t targetNodeId);
        bool SendClearFault(uint16_t targetNodeId);
        bool SendEmergencyStop(uint16_t targetNodeId);
        bool SendSelectControlMode(uint16_t targetNodeId, FocMotorMode mode);
        bool SendSetTorqueSetpoint(uint16_t targetNodeId, foc::Ampere value);
        bool SendSetSpeedSetpoint(uint16_t targetNodeId, foc::RadiansPerSecond value);
        bool SendSetPositionSetpoint(uint16_t targetNodeId, foc::Radians value);

        bool SendSetCurrentBandwidth(uint16_t targetNodeId, float bandwidth);
        bool SendSetSpeedBandwidth(uint16_t targetNodeId, float bandwidth);
        bool SendSetPositionBandwidth(uint16_t targetNodeId, float bandwidth);
        bool SendQueryContractVersion(uint16_t targetNodeId);
        bool SendAlign(uint16_t targetNodeId);
        bool SendIdentifyElectrical(uint16_t targetNodeId);
        bool SendRequestTelemetry(uint16_t targetNodeId);
        bool SendSetEncoderResolution(uint16_t targetNodeId, uint32_t countsPerRevolution);
        bool SendSelectAlgorithm(uint16_t targetNodeId, FocControlLoop loop, uint8_t algorithm);
        bool SendQueryAlgorithms(uint16_t targetNodeId);
        bool SendSetParameter(uint16_t targetNodeId, FocParameter parameter, float value);
        bool SendQueryParameter(uint16_t targetNodeId, FocParameter parameter);
        bool SendMaintenanceCommand(uint16_t targetNodeId, FocMaintenanceCommand command);

        ContractCompatibility Compatibility(uint16_t nodeId) const;

    private:
        bool HandleSelectControlModeResponse(const hal::Can::Message& data);
        bool HandleCategoryError(const hal::Can::Message& data);
        bool HandleTelemetryStatus(const hal::Can::Message& data);
        bool HandleTelemetryElectrical(const hal::Can::Message& data);
        bool HandleContractVersionResponse(const hal::Can::Message& data);
        bool HandleElectricalParamsResponse(const hal::Can::Message& data);
        bool HandleAlgorithmsResponse(const hal::Can::Message& data);
        bool HandleParameterResponse(const hal::Can::Message& data);

        services::CanMessageHandler<FocMotorCategoryClient> selectControlModeResponse{ focSelectControlModeResponseId, *this, &FocMotorCategoryClient::HandleSelectControlModeResponse };
        services::CanMessageHandler<FocMotorCategoryClient> categoryError{ services::canCategoryErrorResponseMessageTypeId, *this, &FocMotorCategoryClient::HandleCategoryError };
        services::CanMessageHandler<FocMotorCategoryClient> telemetryStatus{ focTelemetryStatusResponseId, *this, &FocMotorCategoryClient::HandleTelemetryStatus };
        services::CanMessageHandler<FocMotorCategoryClient> telemetryElectrical{ focTelemetryElectricalResponseId, *this, &FocMotorCategoryClient::HandleTelemetryElectrical };
        services::CanMessageHandler<FocMotorCategoryClient> contractVersionResponse{ focContractVersionResponseId, *this, &FocMotorCategoryClient::HandleContractVersionResponse };
        services::CanMessageHandler<FocMotorCategoryClient> electricalParamsResponse{ focElectricalParamsResponseId, *this, &FocMotorCategoryClient::HandleElectricalParamsResponse };
        services::CanMessageHandler<FocMotorCategoryClient> algorithmsResponse{ focAlgorithmsResponseId, *this, &FocMotorCategoryClient::HandleAlgorithmsResponse };
        services::CanMessageHandler<FocMotorCategoryClient> parameterResponse{ focParameterResponseId, *this, &FocMotorCategoryClient::HandleParameterResponse };

        const CanFrameOrigin& origin;
        NodeContracts contracts;
    };
}
