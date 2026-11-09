#include "tools/can_commander/logic/CanCommandClient.hpp"
#include "can-lite/core/CanFrameCodec.hpp"
#include "core/can/FocMotorWireContract.hpp"

namespace tool
{
    using namespace services;

    CanCommandClient::CanCommandClient(services::CanBusAdapter& adapter)
        : focClient{ adapter }
        , firmwareUpgrade{ focClient.ProtocolClient().Transport(), focClient.ProtocolClient() }
        , tracker{ [this](uint16_t nodeId, bool busy)
            {
                NotifyObservers([nodeId, busy](auto& observer)
                    {
                        observer.OnBusyChanged(nodeId, busy);
                    });
            },
            [this](uint16_t nodeId, uint8_t commandType)
            {
                NotifyObservers([nodeId, commandType](auto& observer)
                    {
                        observer.OnCommandTimeout(nodeId, commandType);
                    });
            } }
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
        return tracker.IsBusy(nodeId);
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

    MotorCommands& CanCommandClient::Commands()
    {
        return commands;
    }

    NodeData& CanCommandClient::Data()
    {
        return nodeData;
    }

    services::FirmwareUpgradeCategoryClient& CanCommandClient::FirmwareUpgrade()
    {
        return firmwareUpgrade;
    }

    const services::CanFrameOrigin& CanCommandClient::FrameOrigin() const
    {
        return focClient.Origin();
    }

    void CanCommandClient::ReportIdentity(uint16_t nodeId, const std::optional<FirmwareIdentity>& identity)
    {
        if (!identity.has_value())
            return;

        NotifyObservers([nodeId, &identity](auto& observer)
            {
                observer.OnFirmwareIdentityReceived(nodeId, *identity);
            });
    }

    void CanCommandClient::RefreshStatus()
    {
        for (const auto nodeId : onlineNodes)
            nodeData.RequestStatus(nodeId);
    }

    void CanCommandClient::MarkOnline(uint16_t nodeId)
    {
        if (nodeId == canBroadcastNodeId || !onlineNodes.insert(nodeId).second)
            return;

        NotifyObservers([nodeId](auto& observer)
            {
                observer.OnNodeOnlineChanged(nodeId, true);
            });
        nodeData.RequestData(nodeId);
    }

    void CanCommandClient::MarkOffline(uint16_t nodeId)
    {
        if (onlineNodes.erase(nodeId) == 0)
            return;

        tracker.Clear(nodeId);
        nodeData.Forget(nodeId);
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

    void CanCommandClient::OnCommandAckTimeout(uint16_t, uint8_t, uint8_t)
    {}

    void CanCommandClient::OnSelectControlModeResponse(uint16_t nodeId, can::FocMotorMode activeMode)
    {
        tracker.Complete(nodeId, can::focSelectControlModeId);
        NotifyObservers([nodeId, activeMode](auto& observer)
            {
                observer.OnControlModeAcknowledged(nodeId, activeMode);
            });
    }

    void CanCommandClient::OnCategoryError(uint16_t nodeId, uint8_t originCommandId, can::FocMotorCategoryError errorCode)
    {
        if (ReadScheduler::IsRead(originCommandId))
        {
            nodeData.ReadAnswered(nodeId, originCommandId);
            return;
        }

        tracker.Complete(nodeId, originCommandId);
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

        nodeData.RequestAfterContract(nodeId);
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
        ReportIdentity(nodeId, nodeData.ParameterReceived(nodeId, parameter, value.value));
    }

    void CanCommandClient::OnFaultRecordResponse(uint16_t nodeId, uint8_t age, can::FocFaultCode code, can::FocMotorMode mode, uint32_t uptimeMs)
    {
        nodeData.FaultRecordReceived(nodeId, age, code, mode, uptimeMs);
    }

    void CanCommandClient::OnFaultConditionsResponse(uint16_t nodeId, uint8_t age, int16_t powerUp, foc::Volts busVoltage, foc::Ampere peakCurrent)
    {
        const auto record = nodeData.FaultConditionsReceived(nodeId, age, powerUp, busVoltage, peakCurrent);
        if (!record.has_value())
            return;

        NotifyObservers([nodeId, &record](auto& observer)
            {
                observer.OnFaultRecordReceived(nodeId, *record);
            });
    }

    bool CanCommandClient::HandleCommandAck(uint32_t id, const services::CanFrame& data)
    {
        if (ExtractCanMessageType(id) != canCommandAckMessageTypeId || ExtractCanCategory(id) != canSystemCategoryId || data.size() < canCommandAckSize)
            return false;

        const auto nodeId = ExtractCanNodeId(id);
        const uint8_t ackCategory = data[0];
        const uint8_t ackMsgType = data[1];
        const auto ackStatus = static_cast<CanAckStatus>(data[2]);

        if (ackCategory == can::focMotorCategoryId && ReadScheduler::IsRead(ackMsgType))
        {
            if (ackStatus != CanAckStatus::success)
                ReportIdentity(nodeId, nodeData.ReadRefused(nodeId, ackMsgType));
            nodeData.ReadAnswered(nodeId, ackMsgType);
            return true;
        }

        if (ackCategory == can::focMotorCategoryId)
            tracker.Complete(nodeId, ackMsgType);

        NotifyObservers([nodeId, ackCategory, ackMsgType, ackStatus](auto& observer)
            {
                observer.OnCommandAck(nodeId, ackCategory, ackMsgType, ackStatus);
            });

        if (ackCategory == can::focMotorCategoryId && ackStatus == CanAckStatus::success)
            nodeData.RequestAfterAccepted(nodeId, ackMsgType);
        return true;
    }

    void CanCommandClient::OnFrameLog(bool transmitted, uint32_t id, const services::CanFrame& data)
    {
        bool consumed{ false };
        if (!transmitted)
        {
            MarkOnline(ExtractCanNodeId(id));

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

            tracker.ClearAll();
        }

        NotifyObservers([connected](auto& observer)
            {
                observer.OnConnectionChanged(connected);
            });
    }
}
