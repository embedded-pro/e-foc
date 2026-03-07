#pragma once

#include <cstdint>

namespace services
{
    static constexpr uint8_t canProtocolVersion = 1;

    static constexpr uint32_t canIdPriorityShift = 24;
    static constexpr uint32_t canIdCategoryShift = 20;
    static constexpr uint32_t canIdMessageTypeShift = 12;
    static constexpr uint32_t canIdNodeIdMask = 0xFFF;
    static constexpr uint32_t canBroadcastNodeId = 0x000;

    enum class CanPriority : uint8_t
    {
        emergency = 0,
        command = 4,
        response = 8,
        telemetry = 12,
        heartbeat = 16
    };

    enum class CanCategory : uint8_t
    {
        motorControl = 0x0,
        pidTuning = 0x1,
        systemParameters = 0x3,
        telemetry = 0x4,
        system = 0x5
    };

    enum class CanMessageType : uint8_t
    {
        // Motor control (category 0x0)
        startMotor = 0x01,
        stopMotor = 0x02,
        emergencyStop = 0x03,
        setControlMode = 0x04,
        setTorqueSetpoint = 0x05,
        setSpeedSetpoint = 0x06,
        setPositionSetpoint = 0x07,

        // PID tuning (category 0x1)
        setCurrentIdPid = 0x11,
        setCurrentIqPid = 0x12,
        setSpeedPid = 0x13,
        setPositionPid = 0x14,

        // System parameters (category 0x3)
        setSupplyVoltage = 0x31,
        setMaxCurrent = 0x32,

        // Telemetry (category 0x4)
        motorStatus = 0x41,
        currentMeasurement = 0x42,
        speedPosition = 0x43,
        busVoltage = 0x44,
        faultEvent = 0x45,

        // System (category 0x5)
        heartbeat = 0x51,
        commandAck = 0x52,
        requestData = 0x53
    };

    enum class DataRequestFlags : uint8_t
    {
        none = 0x00,
        motorStatus = 0x01,
        currentMeasurement = 0x02,
        speedPosition = 0x04,
        busVoltage = 0x08,
        faultEvent = 0x10,
        all = 0x1F
    };

    constexpr DataRequestFlags operator|(DataRequestFlags a, DataRequestFlags b)
    {
        return static_cast<DataRequestFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    constexpr DataRequestFlags operator&(DataRequestFlags a, DataRequestFlags b)
    {
        return static_cast<DataRequestFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    constexpr bool HasFlag(DataRequestFlags flags, DataRequestFlags flag)
    {
        return (flags & flag) != DataRequestFlags::none;
    }

    enum class CanControlMode : uint8_t
    {
        torque = 0,
        speed = 1,
        position = 2
    };

    enum class CanMotorState : uint8_t
    {
        idle = 0,
        running = 1,
        fault = 2,
        aligning = 3
    };

    enum class CanFaultCode : uint8_t
    {
        none = 0,
        overCurrent = 1,
        overVoltage = 2,
        overTemperature = 3,
        sensorFault = 4,
        communicationTimeout = 5
    };

    enum class CanAckStatus : uint8_t
    {
        success = 0,
        unknownCommand = 1,
        invalidPayload = 2,
        invalidState = 3,
        sequenceError = 4,
        rateLimited = 5
    };

    struct DataResponse
    {
        CanMotorState motorState = CanMotorState::idle;
        CanControlMode controlMode = CanControlMode::torque;
        CanFaultCode faultCode = CanFaultCode::none;
        float idCurrent = 0.0f;
        float iqCurrent = 0.0f;
        float speed = 0.0f;
        float position = 0.0f;
        float busVoltage = 0.0f;
    };

    static constexpr uint32_t MakeCanId(CanPriority priority, CanCategory category, CanMessageType messageType, uint16_t nodeId)
    {
        return (static_cast<uint32_t>(priority) << canIdPriorityShift) |
               (static_cast<uint32_t>(category) << canIdCategoryShift) |
               (static_cast<uint32_t>(messageType) << canIdMessageTypeShift) |
               (static_cast<uint32_t>(nodeId) & canIdNodeIdMask);
    }

    static constexpr CanPriority ExtractCanPriority(uint32_t canId)
    {
        return static_cast<CanPriority>((canId >> canIdPriorityShift) & 0x1F);
    }

    static constexpr CanCategory ExtractCanCategory(uint32_t canId)
    {
        return static_cast<CanCategory>((canId >> canIdCategoryShift) & 0xF);
    }

    static constexpr CanMessageType ExtractCanMessageType(uint32_t canId)
    {
        return static_cast<CanMessageType>((canId >> canIdMessageTypeShift) & 0xFF);
    }

    static constexpr uint16_t ExtractCanNodeId(uint32_t canId)
    {
        return static_cast<uint16_t>(canId & canIdNodeIdMask);
    }

    static constexpr int32_t canCurrentScale = 1000;
    static constexpr int32_t canVoltageScale = 100;
    static constexpr int32_t canSpeedScale = 1000;
    static constexpr int32_t canPositionScale = 10000;
    static constexpr int32_t canPidGainScale = 1000;
}
