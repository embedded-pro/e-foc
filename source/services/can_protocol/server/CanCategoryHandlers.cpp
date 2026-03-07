#include "source/services/can_protocol/server/CanCategoryHandlers.hpp"

namespace services
{
    CanMotorControlHandler::CanMotorControlHandler(CanProtocolServer& protocol)
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
            protocol.SendCommandAck(CanCategory::motorControl, messageType, CanAckStatus::unknownCommand);
    }

    void CanMotorControlHandler::HandleStartMotor()
    {
        protocol.NotifyObservers([](auto& observer)
            {
                observer.OnMotorStart();
            });
        protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::startMotor, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleStopMotor()
    {
        protocol.NotifyObservers([](auto& observer)
            {
                observer.OnMotorStop();
            });
        protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::stopMotor, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleEmergencyStop()
    {
        protocol.NotifyObservers([](auto& observer)
            {
                observer.OnEmergencyStop();
            });
        protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::emergencyStop, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleSetControlMode(const hal::Can::Message& data)
    {
        if (data.size() < 2)
        {
            protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::setControlMode, CanAckStatus::invalidPayload);
            return;
        }

        auto mode = static_cast<CanControlMode>(data[1]);
        if (static_cast<uint8_t>(mode) > static_cast<uint8_t>(CanControlMode::position))
        {
            protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::setControlMode, CanAckStatus::invalidPayload);
            return;
        }

        protocol.NotifyObservers([mode](auto& observer)
            {
                observer.OnControlModeChanged(mode);
            });
        protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::setControlMode, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleSetTorqueSetpoint(const hal::Can::Message& data)
    {
        if (data.size() < 5)
        {
            protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::setTorqueSetpoint, CanAckStatus::invalidPayload);
            return;
        }

        float id = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 1), canCurrentScale);
        float iq = CanFrameCodec::Fixed16ToFloat(CanFrameCodec::ReadInt16(data, 3), canCurrentScale);

        protocol.NotifyObservers([id, iq](auto& observer)
            {
                observer.OnTorqueSetpoint(id, iq);
            });
        protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::setTorqueSetpoint, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleSetSpeedSetpoint(const hal::Can::Message& data)
    {
        if (data.size() < 5)
        {
            protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::setSpeedSetpoint, CanAckStatus::invalidPayload);
            return;
        }

        float speed = CanFrameCodec::Fixed32ToFloat(CanFrameCodec::ReadInt32(data, 1), canSpeedScale);

        protocol.NotifyObservers([speed](auto& observer)
            {
                observer.OnSpeedSetpoint(speed);
            });
        protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::setSpeedSetpoint, CanAckStatus::success);
    }

    void CanMotorControlHandler::HandleSetPositionSetpoint(const hal::Can::Message& data)
    {
        if (data.size() < 5)
        {
            protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::setPositionSetpoint, CanAckStatus::invalidPayload);
            return;
        }

        float position = CanFrameCodec::Fixed32ToFloat(CanFrameCodec::ReadInt32(data, 1), canPositionScale);

        protocol.NotifyObservers([position](auto& observer)
            {
                observer.OnPositionSetpoint(position);
            });
        protocol.SendCommandAck(CanCategory::motorControl, CanMessageType::setPositionSetpoint, CanAckStatus::success);
    }

    CanPidTuningHandler::CanPidTuningHandler(CanProtocolServer& protocol)
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
            protocol.SendCommandAck(CanCategory::pidTuning, messageType, CanAckStatus::invalidPayload);
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
            protocol.SendCommandAck(CanCategory::pidTuning, messageType, CanAckStatus::unknownCommand);
            return;
        }

        protocol.SendCommandAck(CanCategory::pidTuning, messageType, CanAckStatus::success);
    }

    CanSystemParametersHandler::CanSystemParametersHandler(CanProtocolServer& protocol)
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
            protocol.SendCommandAck(CanCategory::systemParameters, messageType, CanAckStatus::invalidPayload);
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
            protocol.SendCommandAck(CanCategory::systemParameters, messageType, CanAckStatus::unknownCommand);
            return;
        }

        protocol.SendCommandAck(CanCategory::systemParameters, messageType, CanAckStatus::success);
    }

    CanSystemHandler::CanSystemHandler(CanProtocolServer& protocol)
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
            // Heartbeat handled internally - no observer notification
        }
        else if (messageType == CanMessageType::requestData)
        {
            auto flags = DataRequestFlags::all;
            if (!data.empty())
                flags = static_cast<DataRequestFlags>(data[0]);
            protocol.HandleDataRequest(flags);
        }
        else
            protocol.SendCommandAck(CanCategory::system, messageType, CanAckStatus::unknownCommand);
    }
}
