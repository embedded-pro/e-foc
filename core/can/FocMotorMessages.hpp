#pragma once

#include "core/foc/interfaces/Units.hpp"
#include <cstdint>

namespace can
{
    static constexpr uint8_t focMotorCategoryId = 0x02;

    // Retired and never reused: 0x00 QueryMotorType, 0x07 IdentifyMechanical, 0x80 MotorTypeResponse,
    // 0x87 MechanicalParamsResponse (contract 2.3); 0x88 TelemetryElectrical (contract 2.5)
    static constexpr uint8_t focStartId = 0x01;
    static constexpr uint8_t focStopId = 0x02;
    static constexpr uint8_t focSetPidCurrentId = 0x03;
    static constexpr uint8_t focSetPidSpeedId = 0x04;
    static constexpr uint8_t focSetPidPositionId = 0x05;
    static constexpr uint8_t focIdentifyElectricalId = 0x06;
    static constexpr uint8_t focRequestTelemetryId = 0x08;
    static constexpr uint8_t focSetEncoderResolutionId = 0x09;
    static constexpr uint8_t focAlignId = 0x0A;
    static constexpr uint8_t focClearFaultId = 0x0B;
    static constexpr uint8_t focEmergencyStopId = 0x0C;
    static constexpr uint8_t focConfigureTelemetryRateId = 0x0D;
    static constexpr uint8_t focSelectControlModeId = 0x0E;
    static constexpr uint8_t focSetTorqueSetpointId = 0x0F;
    static constexpr uint8_t focSetSpeedSetpointId = 0x10;
    static constexpr uint8_t focSetPositionSetpointId = 0x11;
    static constexpr uint8_t focQueryContractVersionId = 0x12;
    static constexpr uint8_t focSelectAlgorithmId = 0x13;
    static constexpr uint8_t focQueryAlgorithmsId = 0x14;
    static constexpr uint8_t focSetParameterId = 0x15;
    static constexpr uint8_t focQueryParameterId = 0x16;
    static constexpr uint8_t focCalibrateId = 0x17;
    static constexpr uint8_t focClearCalibrationId = 0x18;
    static constexpr uint8_t focClearDiagnosticsId = 0x19;
    static constexpr uint8_t focQueryFaultHistoryId = 0x1B;
    static constexpr uint8_t focClearFaultHistoryId = 0x1C;
    static constexpr uint8_t focRecalibrateId = 0x1D;

    static constexpr uint8_t focElectricalParamsResponseId = 0x86;
    static constexpr uint8_t focTelemetryStatusResponseId = 0x89;
    static constexpr uint8_t focTelemetryCurrentsId = 0x8A;
    static constexpr uint8_t focSelectControlModeResponseId = 0x8E;
    static constexpr uint8_t focContractVersionResponseId = 0x92;
    static constexpr uint8_t focAlgorithmsResponseId = 0x93;
    static constexpr uint8_t focParameterResponseId = 0x96;
    static constexpr uint8_t focFaultRecordResponseId = 0x9B;
    static constexpr uint8_t focFaultConditionsResponseId = 0x9C;

    static constexpr uint8_t focFaultHistoryDepth = 8;

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
        calibrating = 3,
        partialCalibration = 4,
        ready = 5
    };

    enum class FocFaultCode : uint8_t
    {
        none = 0,
        overCurrent = 1,
        overVoltage = 2,
        underVoltage = 3,
        overTemperature = 4,
        sensorFault = 5,
        stall = 6,
        phaseMissing = 7,
        hardwareFault = 8,
        calibrationFailed = 9,
        overSpeed = 10,
        followingError = 11,
        commandTimeout = 12,
        positionLimit = 13
    };

    enum class FocMotorCategoryError : uint8_t
    {
        busy = 0,
        persistenceFailed = 1,
        modeMismatch = 2,
        calibrationFailed = 3,
        abortedByFault = 4,
        applicationError = 5,
        invalidParameters = 6
    };

    enum class FocControlLoop : uint8_t
    {
        current = 0,
        speed = 1,
        position = 2
    };

    enum class FocCurrentAlgorithm : uint8_t
    {
        pid = 0,
        decoupledPid = 1,
        deadbeat = 2,
        slidingMode = 3
    };

    enum class FocSpeedAlgorithm : uint8_t
    {
        pid = 0,
        lqi = 1,
        adrc = 2,
        twoDof = 3
    };

    enum class FocPositionAlgorithm : uint8_t
    {
        pid = 0,
        cascadeP = 1,
        lqr = 2,
        lqi = 3,
        twoDof = 4
    };

    enum class FocParameter : uint8_t
    {
        currentBandwidth = 0x00,
        currentSwitchingGain = 0x01,
        currentBoundaryLayer = 0x02,
        currentTwoStepDeadbeat = 0x03,
        speedBandwidth = 0x10,
        speedErrorWeight = 0x11,
        speedIntegralWeight = 0x12,
        speedObserverBandwidthRatio = 0x13,
        speedReferenceTimeConstant = 0x14,
        positionBandwidth = 0x20,
        positionErrorWeight = 0x21,
        positionSpeedErrorWeight = 0x22,
        positionIntegralWeight = 0x23,
        positionReferenceTimeConstant = 0x24,
        busVoltageMinimum = 0x30,
        busVoltageMaximum = 0x31,
        resistance = 0x40,
        inductance = 0x41,
        polePairs = 0x42,
        fluxLinkage = 0x43,
        inertia = 0x44,
        viscousFriction = 0x45,
        overspeedLimit = 0x50,
        followingErrorWindow = 0x51,
        followingErrorTime = 0x52,
        speedAcceleration = 0x53,
        speedDeceleration = 0x54,
        commandTimeout = 0x55,
        continuousCurrent = 0x56,
        peakCurrent = 0x57,
        peakCurrentTime = 0x58,
        positionWindow = 0x59,
        positionWindowTime = 0x5A,
        positionMinimum = 0x5B,
        positionMaximum = 0x5C,
        calibrationTemperature = 0x5D,
        temperatureLimit = 0x5E,
        temperatureMargin = 0x5F,
        encoderResolution = 0x60,
        canNodeId = 0x61,
        canBitrate = 0x62,
        supervisionWindow = 0x70,
        supervisionCurrentFraction = 0x71,
        supervisionDemandedSpeed = 0x72,
        supervisionPositionError = 0x73,
        supervisionStillExcursion = 0x74,
        stallDetection = 0x75,
        recalibrationMode = 0x76,
        currentOffsetA = 0x80,
        currentOffsetB = 0x81,
        currentOffsetC = 0x82,
        coggingPeak = 0x83,
        estimatedResistance = 0x90,
        estimatedInductance = 0x91,
        estimatedInertia = 0x92,
        estimatedViscousFriction = 0x93,
        modelResistance = 0x94,
        modelInductance = 0x95,
        modelInertia = 0x96,
        modelViscousFriction = 0x97,
        recalibrationsApplied = 0x98,
        recalibrationsRejected = 0x99,
        loopSamples = 0xA0,
        loopBudgetCycles = 0xA1,
        loopMinimumCycles = 0xA2,
        loopAverageCycles = 0xA3,
        loopMaximumCycles = 0xA4,
        loopOverruns = 0xA5,
        loopDeadlineMisses = 0xA6,
        loopReentries = 0xA7,
        resetCause = 0xB0,
        firmwareVersionMajor = 0xB1,
        firmwareVersionMinor = 0xB2,
        firmwareVersionPatch = 0xB3,
        boardKind = 0xB4,
        stagedVersionMajor = 0xB5,
        stagedVersionMinor = 0xB6,
        stagedVersionPatch = 0xB7,
        targetReached = 0xC0,
        windingTemperature = 0xC1
    };

    struct FocFirmwareIdentity
    {
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint8_t versionPatch;
        uint8_t boardKind;
    };

    enum class FocResetCause : uint8_t
    {
        powerUp = 0,
        brownOut = 1,
        software = 2,
        hardware = 3,
        watchdog = 4
    };

    enum class FocMaintenanceCommand : uint8_t
    {
        calibrate = focCalibrateId,
        clearCalibration = focClearCalibrationId,
        clearDiagnostics = focClearDiagnosticsId,
        recalibrate = focRecalibrateId
    };

    static constexpr uint8_t focParameterAutomaticFlag = 0x01;

    struct FocAlgorithms
    {
        FocMotorMode mode{ FocMotorMode::torque };
        FocCurrentAlgorithm current{ FocCurrentAlgorithm::pid };
        FocSpeedAlgorithm speed{ FocSpeedAlgorithm::pid };
        FocPositionAlgorithm position{ FocPositionAlgorithm::pid };

        friend constexpr bool operator==(const FocAlgorithms&, const FocAlgorithms&) = default;
    };

    struct FocParameterValue
    {
        float value{ 0.0f };
        bool automatic{ false };

        friend constexpr bool operator==(const FocParameterValue&, const FocParameterValue&) = default;
    };

    struct FocFaultRecord
    {
        uint8_t age{ 0 };
        FocFaultCode code{ FocFaultCode::none };
        FocMotorMode mode{ FocMotorMode::torque };
        uint32_t uptimeMs{ 0 };
        int16_t powerUp{ 0 };
        foc::Volts busVoltage{ 0.0f };
        foc::Ampere peakCurrent{ 0.0f };
    };

    static constexpr int32_t focCurrentScale = 10;
    static constexpr int32_t focSpeedScale = 1;
    static constexpr int32_t focPositionScale = 100;
    static constexpr int32_t focResistanceScale = 1000;
    static constexpr int32_t focInductanceScale = 1000;
    static constexpr int32_t focPidScale = 1;
    static constexpr int32_t focStreamCurrentScale = 1000;
    static constexpr int32_t focStreamPositionScale = 10000;
    static constexpr int32_t focRecordedVoltageScale = 100;
    static constexpr int32_t focRecordedCurrentScale = 100;
    static constexpr int32_t focPowerUpScale = 1;

    static constexpr uint32_t focMaxTelemetryRateHz = 1000;
}
