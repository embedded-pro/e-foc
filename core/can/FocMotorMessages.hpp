#pragma once

#include "can-lite/core/CanProtocolDefinitions.hpp"
#include <cstdint>

namespace can
{
    static constexpr uint8_t focMotorCategoryId = 0x02;

    static constexpr uint8_t focQueryMotorTypeId = 0x00;
    static constexpr uint8_t focStartId = 0x01;
    static constexpr uint8_t focStopId = 0x02;
    static constexpr uint8_t focSetPidCurrentId = 0x03;
    static constexpr uint8_t focSetPidSpeedId = 0x04;
    static constexpr uint8_t focSetPidPositionId = 0x05;
    static constexpr uint8_t focIdentifyElectricalId = 0x06;
    static constexpr uint8_t focIdentifyMechanicalId = 0x07;
    static constexpr uint8_t focRequestTelemetryId = 0x08;
    static constexpr uint8_t focSetEncoderResolutionId = 0x09;
    static constexpr uint8_t focClearFaultId = 0x0B;
    static constexpr uint8_t focEmergencyStopId = 0x0C;
    static constexpr uint8_t focConfigureTelemetryRateId = 0x0D;
    static constexpr uint8_t focSelectControlModeId = 0x0E;
    static constexpr uint8_t focSetTorqueSetpointId = 0x0F;
    static constexpr uint8_t focSetSpeedSetpointId = 0x10;
    static constexpr uint8_t focSetPositionSetpointId = 0x11;

    static constexpr uint8_t focMotorTypeResponseId = 0x80;
    static constexpr uint8_t focElectricalParamsResponseId = 0x86;
    static constexpr uint8_t focMechanicalParamsResponseId = 0x87;
    static constexpr uint8_t focTelemetryElectricalResponseId = 0x88;
    static constexpr uint8_t focTelemetryStatusResponseId = 0x89;
    static constexpr uint8_t focSelectControlModeResponseId = 0x8E;

    enum class FocMotorMode : uint8_t
    {
        torque = 0,
        speed = 1,
        position = 2
    };

    enum class FocMotorState : uint8_t
    {
        idle = 0,
        running = 1,
        fault = 2,
        calibrating = 3
    };

    enum class FocFaultCode : uint8_t
    {
        none = 0,
        overCurrent = 1,
        overVoltage = 2,
        underVoltage = 3,
        overTemperature = 4,
        sensorFault = 5
    };

    enum class FocMotorCategoryError : uint8_t
    {
        busy = 0,
        persistenceFailed = 1,
        modeMismatch = 2,
        calibrationFailed = 3,
        abortedByFault = 4,
        applicationError = 5
    };

    static constexpr int32_t focCurrentScale = 10;
    static constexpr int32_t focSpeedScale = 1;
    static constexpr int32_t focPositionScale = 100;
    static constexpr int32_t focVoltageScale = 10;
    static constexpr int32_t focResistanceScale = 1000;
    static constexpr int32_t focInductanceScale = 1000;
    static constexpr int32_t focPidScale = 1;

    static constexpr uint8_t focPidAxisId = 0x00;
    static constexpr uint8_t focPidAxisIq = 0x01;
}
