#include "source/services/can_protocol/CanCategoryHandlers.hpp"

namespace services
{
    CanMotorControlHandler::CanMotorControlHandler(CanProtocolHandler& protocol)
        : protocol(protocol)
    {}

    CanCategory CanMotorControlHandler::Category() const
    {
        return CanCategory::motorControl;
    }

    bool CanMotorControlHandler::RequiresSequenceValidation() const
    {
        return true;
    }

    void CanMotorControlHandler::Handle(CanMessageType messageType, const hal::Can::Message& data)
    {
        if (messageType == CanMessageType::startMotor)
            HandleStartMotor();
        else if (messageType == CanMessageType::stopMotor)
            HandleStopMotor();
        else if (messageType == CanMessageType::emergencyStop)
            HandleEmergencyStop();
        else if (messageType == CanMessageType::setControlMode)
            HandleSetControlMode(data);
        else if (messageType == CanMessageType::setTorqueSetpoint)
            HandleSetTorqueSetpoint(data);
        else if (messageType == CanMessageType::setSpeedSetpoint)
            HandleSetSpeedSetpoint(data);
        else if (messageType == CanMessageType::setPositionSetpoint)
            HandleSetPositionSetpoint(data);
        else
            protocol.SendCommandAck(messageType, CanAckStatus::unknownCommand);
    }

    void CanMotorControlHandler::HandleStartMotor()
    {
        protocol.NotifyObservers([](auto& observer)
            {
                observer.OnMotorStart();
            });
        protocol.SendCommandAck(CanMessageType::startMotor, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleStopMotor()
    {
        protocol.NotifyObservers([](auto& observer)
            {
                observer.OnMotorStop();
            });
        protocol.SendCommandAck(CanMessageType::stopMotor, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleEmergencyStop()
    {
        protocol.NotifyObservers([](auto& observer)
            {
                observer.OnEmergencyStop();
            });
        protocol.SendCommandAck(CanMessageType::emergencyStop, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleSetControlMode(const hal::Can::Message& data)
    {
        if (data.size() < 2)
        {
            protocol.SendCommandAck(CanMessageType::setControlMode, CanAckStatus::invalidPayload);
            return;
        }

        auto mode = static_cast<CanControlMode>(data[1]);
        if (static_cast<uint8_t>(mode) > static_cast<uint8_t>(CanControlMode::position))
        {
            protocol.SendCommandAck(CanMessageType::setControlMode, CanAckStatus::invalidPayload);
            return;
        }

        protocol.NotifyObservers([mode](auto& observer)
            {
                observer.OnControlModeChanged(mode);
            });
        protocol.SendCommandAck(CanMessageType::setControlMode, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleSetTorqueSetpoint(const hal::Can::Message& data)
    {
        if (data.size() < 5)
        {
            protocol.SendCommandAck(CanMessageType::setTorqueSetpoint, CanAckStatus::invalidPayload);
            return;
        }

        float id = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 1), canCurrentScale);
        float iq = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 3), canCurrentScale);

        protocol.NotifyObservers([id, iq](auto& observer)
            {
                observer.OnTorqueSetpoint(id, iq);
            });
        protocol.SendCommandAck(CanMessageType::setTorqueSetpoint, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleSetSpeedSetpoint(const hal::Can::Message& data)
    {
        if (data.size() < 5)
        {
            protocol.SendCommandAck(CanMessageType::setSpeedSetpoint, CanAckStatus::invalidPayload);
            return;
        }

        float speed = CanFrameCodec::Fixed32ToFloat(CanFrameCodec::ReadInt32(data, 1), canSpeedScale);

        protocol.NotifyObservers([speed](auto& observer)
            {
                observer.OnSpeedSetpoint(speed);
            });
        protocol.SendCommandAck(CanMessageType::setSpeedSetpoint, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleSetPositionSetpoint(const hal::Can::Message& data)
    {
        if (data.size() < 5)
        {
            protocol.SendCommandAck(CanMessageType::setPositionSetpoint, CanAckStatus::invalidPayload);
            return;
        }

        float position = CanFrameCodec::Fixed32ToFloat(CanFrameCodec::ReadInt32(data, 1), canPositionScale);

        protocol.NotifyObservers([position](auto& observer)
            {
                observer.OnPositionSetpoint(position);
            });
        protocol.SendCommandAck(CanMessageType::setPositionSetpoint, CanAckStatus::success);
    }

    CanPidTuningHandler::CanPidTuningHandler(CanProtocolHandler& protocol)
        : protocol(protocol)
    {}

    CanCategory CanPidTuningHandler::Category() const
    {
        return CanCategory::pidTuning;
    }

    void CanPidTuningHandler::Handle(CanMessageType messageType, const hal::Can::Message& data)
    {
        if (data.size() < 7)
        {
            protocol.SendCommandAck(messageType, CanAckStatus::invalidPayload);
            return;
        }

        float kp = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 1), canPidGainScale);
        float ki = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 3), canPidGainScale);
        float kd = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 5), canPidGainScale);

        if (messageType == CanMessageType::setCurrentIdPid)
            protocol.NotifyObservers([kp, ki, kd](auto& observer)
                {
                    observer.OnCurrentIdPidChanged(kp, ki, kd);
                });
        else if (messageType == CanMessageType::setCurrentIqPid)
            protocol.NotifyObservers([kp, ki, kd](auto& observer)
                {
                    observer.OnCurrentIqPidChanged(kp, ki, kd);
                });
        else if (messageType == CanMessageType::setSpeedPid)
            protocol.NotifyObservers([kp, ki, kd](auto& observer)
                {
                    observer.OnSpeedPidChanged(kp, ki, kd);
                });
        else if (messageType == CanMessageType::setPositionPid)
            protocol.NotifyObservers([kp, ki, kd](auto& observer)
                {
                    observer.OnPositionPidChanged(kp, ki, kd);
                });
        else
        {
            protocol.SendCommandAck(messageType, CanAckStatus::unknownCommand);
            return;
        }

        protocol.SendCommandAck(messageType, CanAckStatus::success);
    }

    CanMotorParametersHandler::CanMotorParametersHandler(CanProtocolHandler& protocol)
        : protocol(protocol)
    {}

    CanCategory CanMotorParametersHandler::Category() const
    {
        return CanCategory::motorParameters;
    }

    void CanMotorParametersHandler::Handle(CanMessageType messageType, const hal::Can::Message& data)
    {
        if (messageType == CanMessageType::setPolePairs)
            HandleSetPolePairs(data);
        else if (messageType == CanMessageType::setResistance)
            HandleSetResistance(data);
        else if (messageType == CanMessageType::setInductance)
            HandleSetInductance(data);
        else if (messageType == CanMessageType::setFluxLinkage)
            HandleSetFluxLinkage(data);
        else
            protocol.SendCommandAck(messageType, CanAckStatus::unknownCommand);
    }

    void CanMotorParametersHandler::HandleSetPolePairs(const hal::Can::Message& data)
    {
        if (data.size() < 2)
        {
            protocol.SendCommandAck(CanMessageType::setPolePairs, CanAckStatus::invalidPayload);
            return;
        }

        uint8_t polePairs = data[1];
        protocol.NotifyObservers([polePairs](auto& observer)
            {
                observer.OnPolePairsChanged(polePairs);
            });
        protocol.SendCommandAck(CanMessageType::setPolePairs, CanAckStatus::success);
    }

    void CanMotorParametersHandler::HandleSetResistance(const hal::Can::Message& data)
    {
        if (data.size() < 5)
        {
            protocol.SendCommandAck(CanMessageType::setResistance, CanAckStatus::invalidPayload);
            return;
        }

        float value = CanFrameCodec::Fixed32ToFloat(CanFrameCodec::ReadInt32(data, 1), canResistanceScale);
        protocol.NotifyObservers([value](auto& observer)
            {
                observer.OnResistanceChanged(value);
            });
        protocol.SendCommandAck(CanMessageType::setResistance, CanAckStatus::success);
    }

    void CanMotorParametersHandler::HandleSetInductance(const hal::Can::Message& data)
    {
        if (data.size() < 5)
        {
            protocol.SendCommandAck(CanMessageType::setInductance, CanAckStatus::invalidPayload);
            return;
        }

        float value = CanFrameCodec::Fixed32ToFloat(CanFrameCodec::ReadInt32(data, 1), canInductanceScale);
        protocol.NotifyObservers([value](auto& observer)
            {
                observer.OnInductanceChanged(value);
            });
        protocol.SendCommandAck(CanMessageType::setInductance, CanAckStatus::success);
    }

    void CanMotorParametersHandler::HandleSetFluxLinkage(const hal::Can::Message& data)
    {
        if (data.size() < 5)
        {
            protocol.SendCommandAck(CanMessageType::setFluxLinkage, CanAckStatus::invalidPayload);
            return;
        }

        float value = CanFrameCodec::Fixed32ToFloat(CanFrameCodec::ReadInt32(data, 1), canFluxScale);
        protocol.NotifyObservers([value](auto& observer)
            {
                observer.OnFluxLinkageChanged(value);
            });
        protocol.SendCommandAck(CanMessageType::setFluxLinkage, CanAckStatus::success);
    }

    CanSystemParametersHandler::CanSystemParametersHandler(CanProtocolHandler& protocol)
        : protocol(protocol)
    {}

    CanCategory CanSystemParametersHandler::Category() const
    {
        return CanCategory::systemParameters;
    }

    void CanSystemParametersHandler::Handle(CanMessageType messageType, const hal::Can::Message& data)
    {
        if (data.size() < 3)
        {
            protocol.SendCommandAck(messageType, CanAckStatus::invalidPayload);
            return;
        }

        if (messageType == CanMessageType::setSupplyVoltage)
        {
            float value = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 1), canVoltageScale);
            protocol.NotifyObservers([value](auto& observer)
                {
                    observer.OnSupplyVoltageChanged(value);
                });
        }
        else if (messageType == CanMessageType::setMaxCurrent)
        {
            float value = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 1), canCurrentScale);
            protocol.NotifyObservers([value](auto& observer)
                {
                    observer.OnMaxCurrentChanged(value);
                });
        }
        else
        {
            protocol.SendCommandAck(messageType, CanAckStatus::unknownCommand);
            return;
        }

        protocol.SendCommandAck(messageType, CanAckStatus::success);
    }

    CanSystemHandler::CanSystemHandler(CanProtocolHandler& protocol)
        : protocol(protocol)
    {}

    CanCategory CanSystemHandler::Category() const
    {
        return CanCategory::system;
    }

    bool CanSystemHandler::RequiresSequenceValidation() const
    {
        return false;
    }

    void CanSystemHandler::Handle(CanMessageType messageType, const hal::Can::Message& data)
    {
        if (messageType == CanMessageType::heartbeat)
        {
            uint8_t version = 0;
            if (!data.empty())
                version = data[0];
            protocol.NotifyObservers([version](auto& observer)
                {
                    observer.OnHeartbeatReceived(version);
                });
        }
    }
}
