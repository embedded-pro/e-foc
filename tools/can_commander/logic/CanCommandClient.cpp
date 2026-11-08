#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "core/can/FocMotorWireContract.hpp"
#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

namespace tool
{
    using namespace services;

    namespace
    {
        constexpr std::array<can::FocParameter, 7> identityParameters{
            can::FocParameter::firmwareVersionMajor,
            can::FocParameter::firmwareVersionMinor,
            can::FocParameter::firmwareVersionPatch,
            can::FocParameter::boardKind,
            can::FocParameter::stagedVersionMajor,
            can::FocParameter::stagedVersionMinor,
            can::FocParameter::stagedVersionPatch,
        };

        constexpr std::size_t stagedVersionFirst{ 4 };

        std::optional<std::size_t> IdentitySlot(can::FocParameter parameter)
        {
            const auto slot = std::ranges::find(identityParameters, parameter);
            if (slot == identityParameters.end())
                return std::nullopt;
            return static_cast<std::size_t>(slot - identityParameters.begin());
        }

        uint8_t VersionNumber(const std::optional<float>& value)
        {
            return static_cast<uint8_t>(std::lround(value.value_or(0.0f)));
        }
    }

    CanCommandClient::CanCommandClient(services::CanBusAdapter& adapter)
        : focClient{ adapter }
        , firmwareUpgrade{ focClient.ProtocolClient().Transport(), focClient.ProtocolClient() }
        , statusRefresh{ statusRefreshInterval, [this]()
            {
                RefreshStatus();
            } }
    {
        focClient.ProtocolClient().RegisterCategory(firmwareUpgrade);
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

    bool CanCommandClient::IsCompatible(uint16_t nodeId) const
    {
        return Compatibility(nodeId) == can::ContractCompatibility::compatible;
    }

    services::FirmwareUpgradeCategoryClient& CanCommandClient::FirmwareUpgrade()
    {
        return firmwareUpgrade;
    }

    const can::CanFrameOrigin& CanCommandClient::FrameOrigin() const
    {
        return focClient.Origin();
    }

    infra::Duration CanCommandClient::TimeoutFor(uint8_t commandType)
    {
        switch (commandType)
        {
            case can::focAlignId:
            case can::focIdentifyElectricalId:
            case can::focCalibrateId:
                return motionCommandTimeout;
            case can::focStartId:
                return startCommandTimeout;
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

    bool CanCommandClient::SendSetpoint(uint16_t nodeId, can::FocSetpointKind kind, float value)
    {
        return Tracked(nodeId, can::SetpointCommandId(kind), [this, nodeId, kind, value]()
            {
                return focClient.CategoryClient().SendSetpoint(nodeId, can::FocSetpoint{ kind, value });
            });
    }

    bool CanCommandClient::SendSetTorqueSetpoint(uint16_t nodeId, float iqCurrent)
    {
        return SendSetpoint(nodeId, can::FocSetpointKind::torqueCurrent, iqCurrent);
    }

    bool CanCommandClient::SendSetSpeedSetpoint(uint16_t nodeId, float speedRadPerSec)
    {
        return SendSetpoint(nodeId, can::FocSetpointKind::speed, speedRadPerSec);
    }

    bool CanCommandClient::SendSetPositionSetpoint(uint16_t nodeId, float positionRad)
    {
        return SendSetpoint(nodeId, can::FocSetpointKind::position, positionRad);
    }

    bool CanCommandClient::SendSetTorqueSetpointNewtonMeter(uint16_t nodeId, float torqueNewtonMeter)
    {
        return SendSetpoint(nodeId, can::FocSetpointKind::torqueNewtonMeter, torqueNewtonMeter);
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

    bool CanCommandClient::SendClearFaultHistory(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focClearFaultHistoryId, [this, nodeId]()
            {
                return focClient.CategoryClient().SendClearFaultHistory(nodeId);
            });
    }

    bool CanCommandClient::SendRecalibrate(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focRecalibrateId, [this, nodeId]()
            {
                return focClient.CategoryClient().SendMaintenanceCommand(nodeId, can::FocMaintenanceCommand::recalibrate);
            });
    }

    bool CanCommandClient::SendResetModel(uint16_t nodeId)
    {
        return Tracked(nodeId, can::focResetModelId, [this, nodeId]()
            {
                return focClient.CategoryClient().SendMaintenanceCommand(nodeId, can::FocMaintenanceCommand::resetModel);
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
        if (!IsCompatible(nodeId))
            return;

        QueueRead(nodeId, Read{ can::focQueryAlgorithmsId });
        for (const auto& descriptor : can::focParameters)
            if (!descriptor.readOnly)
                QueueRead(nodeId, Read{ can::focQueryParameterId, descriptor.id });
    }

    void CanCommandClient::RequestReadings(uint16_t nodeId)
    {
        if (!IsCompatible(nodeId))
            return;

        for (const auto& descriptor : can::focParameters)
            if (descriptor.readOnly && descriptor.id != can::FocParameter::targetReached && descriptor.id != can::FocParameter::windingTemperature && descriptor.id != can::FocParameter::homed)
                QueueRead(nodeId, Read{ can::focQueryParameterId, descriptor.id });
    }

    void CanCommandClient::RequestFaultHistory(uint16_t nodeId)
    {
        if (!IsCompatible(nodeId))
            return;

        for (uint8_t age = 0; age != can::focFaultHistoryDepth; ++age)
            QueueRead(nodeId, Read{ .commandType = can::focQueryFaultHistoryId, .age = age });
    }

    void CanCommandClient::RequestFirmwareIdentity(uint16_t nodeId)
    {
        if (!IsCompatible(nodeId) || IsIdentityQueued(nodeId))
            return;

        identities.erase(nodeId);
        for (const auto parameter : identityParameters)
            QueueRead(nodeId, Read{ can::focQueryParameterId, parameter });
    }

    bool CanCommandClient::IsIdentityQueued(uint16_t nodeId) const
    {
        const auto node = reads.find(nodeId);
        return node != reads.end() && std::ranges::any_of(node->second.queued, [](const Read& read)
                                          {
                                              return read.commandType == can::focQueryParameterId && IdentitySlot(read.parameter).has_value();
                                          });
    }

    void CanCommandClient::CollectIdentity(uint16_t nodeId, can::FocParameter parameter, float value)
    {
        const auto slot = IdentitySlot(parameter);
        if (!slot.has_value())
            return;

        identities[nodeId].values[*slot] = value;
        ReportIdentityWhenComplete(nodeId);
    }

    void CanCommandClient::OnReadRefused(uint16_t nodeId, uint8_t commandType)
    {
        const auto node = reads.find(nodeId);
        if (commandType != can::focQueryParameterId || node == reads.end() || node->second.inFlight != commandType)
            return;

        const auto slot = IdentitySlot(node->second.inFlightParameter);
        if (!slot.has_value() || *slot < stagedVersionFirst)
            return;

        identities[nodeId].stagingRefused = true;
        ReportIdentityWhenComplete(nodeId);
    }

    void CanCommandClient::ReportIdentityWhenComplete(uint16_t nodeId)
    {
        const auto& partial = identities[nodeId];
        const auto arrived = [&partial](std::size_t first, std::size_t count)
        {
            return std::ranges::all_of(std::span{ partial.values }.subspan(first, count), [](const std::optional<float>& value)
                {
                    return value.has_value();
                });
        };

        const bool staged = arrived(stagedVersionFirst, identityParameters.size() - stagedVersionFirst);
        if (!arrived(0, stagedVersionFirst) || (!staged && !partial.stagingRefused))
            return;

        const auto& values = partial.values;
        const FirmwareIdentity identity{ { VersionNumber(values[0]), VersionNumber(values[1]), VersionNumber(values[2]) }, VersionNumber(values[3]),
            staged ? services::FirmwareVersion{ VersionNumber(values[4]), VersionNumber(values[5]), VersionNumber(values[6]) } : services::FirmwareVersion{}, staged };
        identities.erase(nodeId);
        NotifyObservers([nodeId, &identity](auto& observer)
            {
                observer.OnFirmwareIdentityReceived(nodeId, identity);
            });
    }

    void CanCommandClient::RequestStatus(uint16_t nodeId)
    {
        if (!IsCompatible(nodeId))
            return;

        QueueRead(nodeId, Read{ can::focRequestTelemetryId });
        QueueRead(nodeId, Read{ can::focQueryParameterId, can::FocParameter::targetReached });
        QueueRead(nodeId, Read{ can::focQueryParameterId, can::FocParameter::windingTemperature });
        QueueRead(nodeId, Read{ can::focQueryParameterId, can::FocParameter::homed });
    }

    void CanCommandClient::QueueRead(uint16_t nodeId, Read read)
    {
        auto& node = reads[nodeId];
        const auto queued = std::ranges::any_of(node.queued, [&read](const Read& other)
            {
                return other.commandType == read.commandType && other.parameter == read.parameter && other.age == read.age;
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
                node.inFlightParameter = read.parameter;
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
            case can::focQueryFaultHistoryId:
                return category.SendQueryFaultHistory(nodeId, read.age);
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
        identities.erase(nodeId);
        faultRecords.erase(nodeId);
        if (node == reads.end())
            return;

        node->second.queued.clear();
        node->second.inFlight.reset();
        node->second.timeout.Cancel();
    }

    bool CanCommandClient::IsRead(uint8_t commandType)
    {
        return commandType == can::focQueryAlgorithmsId || commandType == can::focQueryParameterId || commandType == can::focRequestTelemetryId ||
               commandType == can::focQueryFaultHistoryId;
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

    void CanCommandClient::OnTelemetryStatus(uint16_t nodeId, can::FocMotorState state, can::FocFaultCode fault)
    {
        NotifyObservers([nodeId, state, fault](auto& observer)
            {
                observer.OnMotorStatusReceived(nodeId, state, fault);
            });

        if (fault != FocFaultCode::none)
            NotifyObservers([nodeId, fault](auto& observer)
                {
                    observer.OnFaultEventReceived(nodeId, fault);
                });
    }

    void CanCommandClient::OnTelemetryMotion(uint16_t nodeId, foc::RadiansPerSecond speed, foc::Radians position)
    {
        NotifyObservers([nodeId, speed, position](auto& observer)
            {
                observer.OnSpeedPositionReceived(nodeId, speed.Value(), position.Value());
            });
    }

    void CanCommandClient::OnTelemetryCurrents(uint16_t nodeId, uint8_t sequence, foc::Ampere id, foc::Ampere iq, foc::Radians position)
    {
        NotifyObservers([nodeId, sequence, id, iq, position](auto& observer)
            {
                observer.OnCurrentStreamReceived(nodeId, sequence, id.Value(), iq.Value(), position.Value());
            });
    }

    void CanCommandClient::OnContractVersionResponse(uint16_t nodeId, uint8_t major, uint8_t minor)
    {
        const bool compatible = can::IsCompatibleContract(major, minor);

        NotifyObservers([nodeId, major, minor, compatible](auto& observer)
            {
                observer.OnContractVersion(nodeId, major, minor, compatible);
            });

        RequestSettings(nodeId);
        RequestReadings(nodeId);
        RequestFaultHistory(nodeId);
        RequestFirmwareIdentity(nodeId);
        RequestStatus(nodeId);
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
        CollectIdentity(nodeId, parameter, value.value);
    }

    void CanCommandClient::OnFaultRecordResponse(uint16_t nodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs)
    {
        faultRecords[nodeId] = can::FocFaultRecord{ .age = age, .code = code, .mode = mode, .uptimeMs = uptimeMs };
    }

    void CanCommandClient::OnFaultConditionsResponse(uint16_t nodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent)
    {
        const auto partial = faultRecords.find(nodeId);
        if (partial == faultRecords.end() || partial->second.age != age)
            return;

        auto record = partial->second;
        faultRecords.erase(partial);
        record.powerUp = powerUp;
        record.busVoltage = busVoltage;
        record.peakCurrent = peakCurrent;
        NotifyObservers([nodeId, &record](auto& observer)
            {
                observer.OnFaultRecordReceived(nodeId, record);
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
            if (ackStatus != CanAckStatus::success)
                OnReadRefused(nodeId, ackMsgType);
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
            consumed = msgType == can::focTelemetryStatusResponseId || msgType == can::focTelemetryMotionId || msgType == can::focTelemetryCurrentsId || HandleCommandAck(id, data);
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
