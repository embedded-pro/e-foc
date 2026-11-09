#include "tools/can_commander/logic/NodeData.hpp"
#include "core/can/FocMotorWireContract.hpp"

namespace tool
{
    NodeData::NodeData(can::FocMotorCategoryClient& category)
        : category{ category }
        , reads{ [this](uint16_t nodeId, const NodeRead& read)
            {
                return Send(nodeId, read);
            } }
    {}

    bool NodeData::RequestData(uint16_t nodeId)
    {
        return category.SendQueryContractVersion(nodeId);
    }

    void NodeData::RequestSettings(uint16_t nodeId)
    {
        if (!IsCompatible(nodeId))
            return;

        reads.Queue(nodeId, NodeRead{ can::focQueryAlgorithmsId });
        for (const auto& descriptor : can::focParameters)
            if (!descriptor.readOnly)
                reads.Queue(nodeId, NodeRead{ can::focQueryParameterId, descriptor.id });
    }

    void NodeData::RequestReadings(uint16_t nodeId)
    {
        if (!IsCompatible(nodeId))
            return;

        for (const auto& descriptor : can::focParameters)
            if (descriptor.readOnly && descriptor.id != can::FocParameter::targetReached && descriptor.id != can::FocParameter::windingTemperature && descriptor.id != can::FocParameter::homed)
                reads.Queue(nodeId, NodeRead{ can::focQueryParameterId, descriptor.id });
    }

    void NodeData::RequestStatus(uint16_t nodeId)
    {
        if (!IsCompatible(nodeId))
            return;

        reads.Queue(nodeId, NodeRead{ can::focRequestTelemetryId });
        reads.Queue(nodeId, NodeRead{ can::focQueryParameterId, can::FocParameter::targetReached });
        reads.Queue(nodeId, NodeRead{ can::focQueryParameterId, can::FocParameter::windingTemperature });
        reads.Queue(nodeId, NodeRead{ can::focQueryParameterId, can::FocParameter::homed });
    }

    void NodeData::RequestFirmwareIdentity(uint16_t nodeId)
    {
        if (category.Compatibility(nodeId) == can::ContractCompatibility::unknown || IsIdentityQueued(nodeId))
            return;

        identities.Begin(nodeId);
        for (const auto parameter : FirmwareIdentityCollector::parameters)
            reads.Queue(nodeId, NodeRead{ can::focQueryParameterId, parameter });
    }

    void NodeData::RequestFaultHistory(uint16_t nodeId)
    {
        if (!IsCompatible(nodeId))
            return;

        for (uint8_t age = 0; age != can::focFaultHistoryDepth; ++age)
            reads.Queue(nodeId, NodeRead{ .commandType = can::focQueryFaultHistoryId, .age = age });
    }

    void NodeData::RequestAfterContract(uint16_t nodeId)
    {
        RequestSettings(nodeId);
        RequestReadings(nodeId);
        RequestFaultHistory(nodeId);
        RequestFirmwareIdentity(nodeId);
        RequestStatus(nodeId);
    }

    void NodeData::RequestAfterAccepted(uint16_t nodeId, uint8_t commandType)
    {
        switch (commandType)
        {
            case can::focSelectControlModeId:
            case can::focSelectAlgorithmId:
            case can::focAlignId:
            case can::focIdentifyElectricalId:
                RequestSettings(nodeId);
                break;
            case can::focCalibrateId:
            case can::focClearCalibrationId:
                RequestSettings(nodeId);
                RequestReadings(nodeId);
                break;
            case can::focClearDiagnosticsId:
            case can::focRecalibrateId:
            case can::focResetModelId:
                RequestReadings(nodeId);
                break;
            case can::focClearFaultHistoryId:
                RequestFaultHistory(nodeId);
                break;
            default:
                break;
        }

        RequestStatus(nodeId);
    }

    void NodeData::ReadAnswered(uint16_t nodeId, uint8_t commandType)
    {
        reads.Complete(nodeId, commandType);
    }

    std::optional<FirmwareIdentity> NodeData::ReadRefused(uint16_t nodeId, uint8_t commandType)
    {
        if (commandType != can::focQueryParameterId)
            return std::nullopt;

        const auto parameter = reads.InFlightParameter(nodeId, commandType);
        if (!parameter.has_value())
            return std::nullopt;

        return identities.Refused(nodeId, *parameter);
    }

    std::optional<FirmwareIdentity> NodeData::ParameterReceived(uint16_t nodeId, can::FocParameter parameter, float value)
    {
        return identities.Collect(nodeId, parameter, value);
    }

    void NodeData::FaultRecordReceived(uint16_t nodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs)
    {
        faultRecords.Record(nodeId, age, code, mode, uptimeMs);
    }

    std::optional<can::FocFaultRecord> NodeData::FaultConditionsReceived(uint16_t nodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent)
    {
        return faultRecords.Conditions(nodeId, age, powerUp, busVoltage, peakCurrent);
    }

    void NodeData::Forget(uint16_t nodeId)
    {
        identities.Forget(nodeId);
        faultRecords.Forget(nodeId);
        reads.Forget(nodeId);
    }

    bool NodeData::IsCompatible(uint16_t nodeId) const
    {
        return category.Compatibility(nodeId) == can::ContractCompatibility::compatible;
    }

    bool NodeData::IsIdentityQueued(uint16_t nodeId) const
    {
        return reads.AnyQueued(nodeId, [](const NodeRead& read)
            {
                return read.commandType == can::focQueryParameterId && FirmwareIdentityCollector::IsIdentityParameter(read.parameter);
            });
    }

    bool NodeData::Send(uint16_t nodeId, const NodeRead& read)
    {
        switch (read.commandType)
        {
            case can::focQueryAlgorithmsId:
                return category.SendQueryAlgorithms(nodeId);
            case can::focQueryParameterId:
                return category.SendQueryParameter(nodeId, read.parameter);
            case can::focQueryFaultHistoryId:
                return category.SendQueryFaultHistory(nodeId, read.age);
            default:
                return category.SendRequestTelemetry(nodeId);
        }
    }
}
