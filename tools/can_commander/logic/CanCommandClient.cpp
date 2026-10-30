#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "can-lite/core/CanPayload.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include <algorithm>
#include <utility>

namespace tool
{
    using namespace services;

    CanCommandClient::CanCommandClient(services::CanBusAdapter& adapter)
        : focClient{ adapter }
        , statusRefresh{ statusRefreshInterval, [this]()
            {
                RefreshStatus();
            } }
    {
        CanProtocolClientObserver::Attach(focClient.ProtocolClient());
        can::FocMotorCategoryClientObserver::Attach(focClient.CategoryClient());
        services::CanBusAdapterObserver::Attach(adapter);
    }

    CanCommandClient::~CanCommandClient()
    {
        can::FocMotorCategoryClientObserver::Detach();
        CanProtocolClientObserver::Detach();
        services::CanBusAdapterObserver::Detach();
    }

    bool CanCommandClient::IsBusy(uint16_t nodeId) const
    {
        const auto node = commands.find(nodeId);
        return node != commands.end() && node->second.busy;
    }

    bool CanCommandClient::IsOnline(uint16_t nodeId) const
    {
        return onlineNodes.contains(nodeId);
    }

    std::set<uint16_t> CanCommandClient::OnlineNodes() const
    {
        return onlineNodes;
    }

    can::ContractCompatibility CanCommandClient::Compatibility(uint16_t nodeId) const
    {
        return focClient.CategoryClient().Compatibility(nodeId);
    }

    bool CanCommandClient::SupportsSettings(uint16_t nodeId) const
    {
        return SupportsMinor(nodeId, settingsContractMinor);
    }

    bool CanCommandClient::SupportsDiagnostics(uint16_t nodeId) const
    {
        return SupportsMinor(nodeId, diagnosticsContractMinor);
    }

    bool CanCommandClient::SupportsMinor(uint16_t nodeId, uint8_t minimumMinor) const
    {
        const auto minor = contractMinors.find(nodeId);
        return Compatibility(nodeId) == can::ContractCompatibility::compatible && minor != contractMinors.end() && minor->second >= minimumMinor;
    }

    infra::Duration CanCommandClient::TimeoutFor(uint8_t commandType)
    {
        switch (commandType)
        {
            case can::focAlignId:
            case can::focIdentifyElectricalId:
            case can::focCalibrateId:
                return motionCommandTimeout;
            default:
                return commandTimeout;
        }
    }

    void CanCommandClient::MarkBusy(uint16_t nodeId, uint8_t commandType)
    {
        auto& node = commands[nodeId];
        node.pendingCommand = commandType;
        node.timeout.Start(TimeoutFor(commandType), [this, nodeId, commandType]()
            {
                ClearBusy(nodeId);
                NotifyObservers([nodeId, commandType](auto& observer)
                    {
                        observer.OnCommandTimeout(nodeId, commandType);
                    });
            });

        if (!std::exchange(node.busy, true))
            NotifyObservers([nodeId](auto& observer)
                {
                    observer.OnBusyChanged(nodeId, true);
                });
    }

    void CanCommandClient::ClearBusy(uint16_t nodeId)
    {
        const auto node = commands.find(nodeId);
        if (node == commands.end() || !node->second.busy)
            return;

        node->second.busy = false;
        node->second.timeout.Cancel();
        NotifyObservers([nodeId](auto& observer)
            {
                observer.OnBusyChanged(nodeId, false);
            });
    }

    void CanCommandClient::CompleteCommand(uint16_t nodeId, uint8_t commandType)
    {
        const auto node = commands.find(nodeId);
        if (node != commands.end() && node->second.pendingCommand == commandType)
            ClearBusy(nodeId);
    }

    template<class Send>
    bool CanCommandClient::Tracked(uint16_t nodeId, uint8_t commandType, Send send)
    {
        if (!send())
            return false;

        MarkBusy(nodeId, commandType);
        return true;
    }

    bool CanCommandClient::SendStartMotor(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focStartId, [&]()
            {
                return focClient.CategoryClient().SendStart(nodeId);
            });
    }

    bool CanCommandClient::SendStopMotor(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focStopId, [&]()
            {
                return focClient.CategoryClient().SendStop(nodeId);
            });
    }

    bool CanCommandClient::SendEmergencyStop(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focEmergencyStopId, [&]()
            {
                return focClient.CategoryClient().SendEmergencyStop(nodeId);
            });
    }

    bool CanCommandClient::SendSetControlMode(uint16_t nodeId, can::FocMotorMode mode)
    {
        return Tracked(nodeId, can::focSelectControlModeId, [&]()
            {
                return focClient.CategoryClient().SendSelectControlMode(nodeId, mode);
            });
    }

    bool CanCommandClient::SendSetTorqueSetpoint(uint16_t nodeId, float iqCurrent)
    {
        return Tracked(nodeId, can::focSetTorqueSetpointId, [&]()
            {
                return focClient.CategoryClient().SendSetTorqueSetpoint(nodeId, foc::Ampere{ iqCurrent });
            });
    }

    bool CanCommandClient::SendSetSpeedSetpoint(uint16_t nodeId, float speedRadPerSec)
    {
        return Tracked(nodeId, can::focSetSpeedSetpointId, [&]()
            {
                return focClient.CategoryClient().SendSetSpeedSetpoint(nodeId, foc::RadiansPerSecond{ speedRadPerSec });
            });
    }

    bool CanCommandClient::SendSetPositionSetpoint(uint16_t nodeId, float positionRad)
    {
        return Tracked(nodeId, can::focSetPositionSetpointId, [&]()
            {
                return focClient.CategoryClient().SendSetPositionSetpoint(nodeId, foc::Radians{ positionRad });
            });
    }

    bool CanCommandClient::SendSetCurrentBandwidth(uint16_t nodeId, float bandwidth)
    {
        return Tracked(nodeId, can::focSetPidCurrentId, [&]()
            {
                return focClient.CategoryClient().SendSetCurrentBandwidth(nodeId, bandwidth);
            });
    }

    bool CanCommandClient::SendSetSpeedBandwidth(uint16_t nodeId, float bandwidth)
    {
        return Tracked(nodeId, can::focSetPidSpeedId, [&]()
            {
                return focClient.CategoryClient().SendSetSpeedBandwidth(nodeId, bandwidth);
            });
    }

    bool CanCommandClient::SendSetPositionBandwidth(uint16_t nodeId, float bandwidth)
    {
        return Tracked(nodeId, can::focSetPidPositionId, [&]()
            {
                return focClient.CategoryClient().SendSetPositionBandwidth(nodeId, bandwidth);
            });
    }

    bool CanCommandClient::SendClearFault(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focClearFaultId, [this, nodeId]()
            {
                return focClient.CategoryClient().SendClearFault(nodeId);
            });
    }

    bool CanCommandClient::SendAlign(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focAlignId, [this, nodeId]()
            {
                return focClient.CategoryClient().SendAlign(nodeId);
            });
    }

    bool CanCommandClient::SendIdentifyElectrical(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focIdentifyElectricalId, [this, nodeId]()
            {
                return focClient.CategoryClient().SendIdentifyElectrical(nodeId);
            });
    }

    bool CanCommandClient::SendCalibrate(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focCalibrateId, [this, nodeId]()
            {
                return focClient.CategoryClient().SendMaintenanceCommand(nodeId, can::FocMaintenanceCommand::calibrate);
            });
    }

    bool CanCommandClient::SendClearCalibration(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focClearCalibrationId, [this, nodeId]()
            {
                return focClient.CategoryClient().SendMaintenanceCommand(nodeId, can::FocMaintenanceCommand::clearCalibration);
            });
    }

    bool CanCommandClient::SendClearDiagnostics(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focClearDiagnosticsId, [this, nodeId]()
            {
                return focClient.CategoryClient().SendMaintenanceCommand(nodeId, can::FocMaintenanceCommand::clearDiagnostics);
            });
    }

    bool CanCommandClient::SendSetEncoderResolution(uint16_t nodeId, uint32_t countsPerRevolution)
    {
        return Tracked(nodeId, can::focSetEncoderResolutionId, [this, nodeId, countsPerRevolution]()
            {
                return focClient.CategoryClient().SendSetEncoderResolution(nodeId, countsPerRevolution);
            });
    }

    bool CanCommandClient::SendSelectAlgorithm(uint16_t nodeId, can::FocControlLoop loop, uint8_t algorithm)
    {
        return Tracked(nodeId, can::focSelectAlgorithmId, [this, nodeId, loop, algorithm]()
            {
                return focClient.CategoryClient().SendSelectAlgorithm(nodeId, loop, algorithm);
            });
    }

    bool CanCommandClient::SendSetParameter(uint16_t nodeId, can::FocParameter parameter, float value)
    {
        return Tracked(nodeId, can::focSetParameterId, [this, nodeId, parameter, value]()
            {
                return focClient.CategoryClient().SendSetParameter(nodeId, parameter, value);
            });
    }

    bool CanCommandClient::RequestData(uint16_t nodeId)
    {
        return focClient.CategoryClient().SendQueryContractVersion(nodeId);
    }

    void CanCommandClient::RequestSettings(uint16_t nodeId)
    {
        if (!SupportsSettings(nodeId))
            return;

        QueueRead(nodeId, Read{ can::focQueryAlgorithmsId });
        for (const auto& descriptor : can::focParameters)
            if (!descriptor.readOnly)
                QueueRead(nodeId, Read{ can::focQueryParameterId, descriptor.id });
    }

    void CanCommandClient::RequestReadings(uint16_t nodeId)
    {
        if (!SupportsDiagnostics(nodeId))
            return;

        for (const auto& descriptor : can::focParameters)
            if (descriptor.readOnly)
                QueueRead(nodeId, Read{ can::focQueryParameterId, descriptor.id });
    }

    void CanCommandClient::RequestStatus(uint16_t nodeId)
    {
        if (Compatibility(nodeId) == can::ContractCompatibility::compatible)
            QueueRead(nodeId, Read{ can::focRequestTelemetryId });
    }

    void CanCommandClient::QueueRead(uint16_t nodeId, Read read)
    {
        auto& node = reads[nodeId];
        const auto queued = std::ranges::any_of(node.queued, [&read](const Read& other)
            {
                return other.commandType == read.commandType && other.parameter == read.parameter;
            });

        if (!queued)
            node.queued.push_back(read);

        if (!node.inFlight.has_value())
            SendNextRead(nodeId);
    }

    void CanCommandClient::SendNextRead(uint16_t nodeId)
    {
        auto& node = reads[nodeId];
        while (!node.queued.empty())
        {
            const auto read = node.queued.front();
            node.queued.pop_front();

            if (SendRead(nodeId, read))
            {
                node.inFlight = read.commandType;
                node.timeout.Start(readTimeout, [this, nodeId, commandType = read.commandType]()
                    {
                        CompleteRead(nodeId, commandType);
                    });
                return;
            }
        }
    }

    bool CanCommandClient::SendRead(uint16_t nodeId, const Read& read)
    {
        auto& category = focClient.CategoryClient();
        switch (read.commandType)
        {
            case can::focQueryAlgorithmsId:
                return category.SendQueryAlgorithms(nodeId);
            case can::focQueryParameterId:
                return category.SendQueryParameter(nodeId, read.parameter);
            default:
                return category.SendRequestTelemetry(nodeId);
        }
    }

    void CanCommandClient::CompleteRead(uint16_t nodeId, uint8_t commandType)
    {
        const auto node = reads.find(nodeId);
        if (node == reads.end() || node->second.inFlight != commandType)
            return;

        node->second.inFlight.reset();
        node->second.timeout.Cancel();
        SendNextRead(nodeId);
    }

    void CanCommandClient::ForgetReads(uint16_t nodeId)
    {
        const auto node = reads.find(nodeId);
        if (node == reads.end())
            return;

        node->second.queued.clear();
        node->second.inFlight.reset();
        node->second.timeout.Cancel();
    }

    bool CanCommandClient::IsRead(uint8_t commandType)
    {
        return commandType == can::focQueryAlgorithmsId || commandType == can::focQueryParameterId || commandType == can::focRequestTelemetryId;
    }

    void CanCommandClient::OnAccepted(uint16_t nodeId, uint8_t commandType)
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
                RequestReadings(nodeId);
                break;
            default:
                break;
        }

        RequestStatus(nodeId);
    }

    void CanCommandClient::RefreshStatus()
    {
        for (const auto nodeId : onlineNodes)
            RequestStatus(nodeId);
    }

    void CanCommandClient::MarkOnline(uint16_t nodeId)
    {
        if (nodeId == canBroadcastNodeId || !onlineNodes.insert(nodeId).second)
            return;

        NotifyObservers([nodeId](auto& observer)
            {
                observer.OnNodeOnlineChanged(nodeId, true);
            });
        RequestData(nodeId);
    }

    void CanCommandClient::MarkOffline(uint16_t nodeId)
    {
        if (onlineNodes.erase(nodeId) == 0)
            return;

        ClearBusy(nodeId);
        ForgetReads(nodeId);
        NotifyObservers([nodeId](auto& observer)
            {
                observer.OnNodeOnlineChanged(nodeId, false);
            });
    }

    void CanCommandClient::OnServerOnline(uint16_t nodeId)
    {
        MarkOnline(nodeId);
    }

    void CanCommandClient::OnServerOffline(uint16_t nodeId)
    {
        MarkOffline(nodeId);
    }

    void CanCommandClient::OnCommandAckTimeout(uint16_t /*nodeId*/, uint8_t /*category*/, uint8_t /*messageType*/)
    {}

    void CanCommandClient::OnSelectControlModeResponse(uint16_t nodeId, can::FocMotorMode activeMode)
    {
        CompleteCommand(nodeId, can::focSelectControlModeId);
        NotifyObservers([nodeId, activeMode](auto& observer)
            {
                observer.OnControlModeAcknowledged(nodeId, activeMode);
            });
    }

    void CanCommandClient::OnCategoryError(uint16_t nodeId, uint8_t originCommandId, can::FocMotorCategoryError errorCode)
    {
        if (IsRead(originCommandId))
        {
            CompleteRead(nodeId, originCommandId);
            return;
        }

        CompleteCommand(nodeId, originCommandId);
        NotifyObservers([nodeId, originCommandId, errorCode](auto& observer)
            {
                observer.OnCommandRejected(nodeId, originCommandId, errorCode);
            });
    }

    void CanCommandClient::OnTelemetryStatus(uint16_t nodeId, const hal::Can::Message& msg)
    {
        DecodeTelemetryStatus(nodeId, msg);
    }

    void CanCommandClient::OnTelemetryElectrical(uint16_t nodeId, const hal::Can::Message& msg)
    {
        DecodeTelemetryElectrical(nodeId, msg);
    }

    void CanCommandClient::OnContractVersionResponse(uint16_t nodeId, uint8_t major, uint8_t minor)
    {
        const bool compatible = major == can::focContractVersionMajor;
        contractMinors[nodeId] = minor;

        NotifyObservers([nodeId, major, minor, compatible](auto& observer)
            {
                observer.OnContractVersion(nodeId, major, minor, compatible);
            });

        RequestSettings(nodeId);
        RequestReadings(nodeId);
        RequestStatus(nodeId);
    }

    void CanCommandClient::OnElectricalParamsResponse(uint16_t nodeId, foc::Ohm resistance, foc::MilliHenry inductance, uint8_t polePairs)
    {
        NotifyObservers([nodeId, resistance, inductance, polePairs](auto& observer)
            {
                observer.OnElectricalParametersReceived(nodeId, resistance.Value(), inductance.Value(), polePairs);
            });
    }

    void CanCommandClient::OnAlgorithmsResponse(uint16_t nodeId, const can::FocAlgorithms& algorithms)
    {
        NotifyObservers([nodeId, &algorithms](auto& observer)
            {
                observer.OnAlgorithmsReceived(nodeId, algorithms);
            });
    }

    void CanCommandClient::OnParameterResponse(uint16_t nodeId, can::FocParameter parameter, can::FocParameterValue value)
    {
        NotifyObservers([nodeId, parameter, value](auto& observer)
            {
                observer.OnParameterReceived(nodeId, parameter, value);
            });
    }

    void CanCommandClient::DecodeTelemetryStatus(uint16_t nodeId, const hal::Can::Message& msg) const
    {
        if (!can::PayloadExact(msg, can::focTelemetryStatusResponseId))
            return;

        services::CanPayloadReader reader{ msg };
        const auto state = static_cast<FocMotorState>(reader.ReadUInt8());
        const auto fault = static_cast<FocFaultCode>(reader.ReadUInt8());
        const auto speed = reader.ReadFixed16(can::focSpeedScale);
        const auto position = reader.ReadFixed16(can::focPositionScale);

        NotifyObservers([nodeId, state, fault](auto& observer)
            {
                observer.OnMotorStatusReceived(nodeId, state, fault);
            });
        NotifyObservers([nodeId, speed, position](auto& observer)
            {
                observer.OnSpeedPositionReceived(nodeId, speed, position);
            });
        if (fault != FocFaultCode::none)
        {
            NotifyObservers([nodeId, fault](auto& observer)
                {
                    observer.OnFaultEventReceived(nodeId, fault);
                });
        }
    }

    void CanCommandClient::DecodeTelemetryElectrical(uint16_t nodeId, const hal::Can::Message& msg) const
    {
        if (!can::PayloadExact(msg, can::focTelemetryElectricalResponseId))
            return;

        services::CanPayloadReader reader{ msg };
        const auto voltage = reader.ReadFixed16(can::focVoltageScale);
        reader.Skip(2);
        const auto iq = reader.ReadFixed16(can::focCurrentScale);
        const auto id = reader.ReadFixed16(can::focCurrentScale);

        NotifyObservers([nodeId, id, iq](auto& observer)
            {
                observer.OnCurrentMeasurementReceived(nodeId, id, iq);
            });
        NotifyObservers([nodeId, voltage](auto& observer)
            {
                observer.OnBusVoltageReceived(nodeId, voltage);
            });
    }

    bool CanCommandClient::HandleCommandAck(uint32_t id, const services::CanFrame& data)
    {
        if (ExtractCanMessageType(id) != canCommandAckMessageTypeId || ExtractCanCategory(id) != canSystemCategoryId || data.size() < canCommandAckSize)
            return false;

        const auto nodeId = static_cast<uint16_t>(ExtractCanNodeId(id));
        const uint8_t ackCategory = data[0];
        const uint8_t ackMsgType = data[1];
        const auto ackStatus = static_cast<CanAckStatus>(data[2]);

        if (ackCategory == can::focMotorCategoryId && IsRead(ackMsgType))
        {
            CompleteRead(nodeId, ackMsgType);
            return true;
        }

        if (ackCategory == can::focMotorCategoryId)
            CompleteCommand(nodeId, ackMsgType);

        NotifyObservers([nodeId, ackCategory, ackMsgType, ackStatus](auto& observer)
            {
                observer.OnCommandAck(nodeId, ackCategory, ackMsgType, ackStatus);
            });

        if (ackCategory == can::focMotorCategoryId && ackStatus == CanAckStatus::success)
            OnAccepted(nodeId, ackMsgType);
        return true;
    }

    void CanCommandClient::OnFrameLog(bool transmitted, uint32_t id, const services::CanFrame& data)
    {
        bool consumed{ false };
        if (!transmitted)
        {
            MarkOnline(static_cast<uint16_t>(ExtractCanNodeId(id)));

            const uint8_t msgType = ExtractCanMessageType(id);
            consumed = msgType == can::focTelemetryStatusResponseId || msgType == can::focTelemetryElectricalResponseId || HandleCommandAck(id, data);
        }

        if (!consumed)
        {
            NotifyObservers([transmitted, id, &data](auto& observer)
                {
                    observer.OnFrameLog(transmitted, id, data);
                });
        }
    }

    void CanCommandClient::OnError(infra::BoundedConstString message)
    {
        NotifyObservers([&message](auto& observer)
            {
                observer.OnAdapterError(message);
            });
    }

    void CanCommandClient::OnConnectionChanged(bool connected)
    {
        if (!connected)
        {
            while (!onlineNodes.empty())
                MarkOffline(*onlineNodes.begin());

            for (auto& [nodeId, command] : commands)
                ClearBusy(nodeId);
        }

        NotifyObservers([connected](auto& observer)
            {
                observer.OnConnectionChanged(connected);
            });
    }
}
