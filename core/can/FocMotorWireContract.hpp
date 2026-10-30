#pragma once

#include "can-lite/core/CanPayload.hpp"
#include "can-lite/core/CanProtocolDefinitions.hpp"
#include "core/can/FocMotorMessages.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/math/ParameterValidation.hpp"
#include "hal/interfaces/Can.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace can
{
    static constexpr uint8_t focContractVersionMajor = 2;
    static constexpr uint8_t focContractVersionMinor = 7;

    enum class FieldType : uint8_t
    {
        uint8,
        fixed16,
        uint32,
        float32
    };

    constexpr uint8_t SizeOf(FieldType type)
    {
        switch (type)
        {
            case FieldType::uint8:
                return 1;
            case FieldType::fixed16:
                return 2;
            default:
                return 4;
        }
    }

    enum class Direction : uint8_t
    {
        command,
        response,
        telemetry
    };

    struct FieldDescriptor
    {
        const char* name;
        FieldType type;
        int32_t scale;
        const char* unit;
    };

    struct MessageDescriptor
    {
        uint8_t id;
        const char* name;
        Direction direction;
        const FieldDescriptor* fields;
        uint8_t fieldCount;

        constexpr bool SequencePrefixed() const
        {
            return direction == Direction::command;
        }

        constexpr uint8_t PayloadSize() const
        {
            uint8_t size = SequencePrefixed() ? 1 : 0;

            for (uint8_t i = 0; i != fieldCount; ++i)
                size += SizeOf(fields[i].type);

            return size;
        }
    };

    inline constexpr std::array<FieldDescriptor, 1> bandwidthFields{ { { "bandwidth", FieldType::fixed16, focPidScale, "rad/s" } } };
    inline constexpr std::array<FieldDescriptor, 1> torqueFields{ { { "iq", FieldType::fixed16, focCurrentScale, "A" } } };
    inline constexpr std::array<FieldDescriptor, 1> speedFields{ { { "speed", FieldType::fixed16, focSpeedScale, "rad/s" } } };
    inline constexpr std::array<FieldDescriptor, 1> positionFields{ { { "position", FieldType::fixed16, focPositionScale, "rad" } } };
    inline constexpr std::array<FieldDescriptor, 1> modeFields{ { { "mode", FieldType::uint8, 1, "enum" } } };
    inline constexpr std::array<FieldDescriptor, 1> resolutionFields{ { { "resolution", FieldType::uint32, 1, "counts/rev" } } };
    inline constexpr std::array<FieldDescriptor, 1> telemetryRateFields{ { { "rate", FieldType::uint32, 1, "Hz" } } };

    inline constexpr std::array<FieldDescriptor, 2> selectAlgorithmFields{ {
        { "loop", FieldType::uint8, 1, "enum" },
        { "algorithm", FieldType::uint8, 1, "enum" },
    } };

    inline constexpr std::array<FieldDescriptor, 2> setParameterFields{ {
        { "parameter", FieldType::uint8, 1, "enum" },
        { "value", FieldType::float32, 1, "parameter" },
    } };

    inline constexpr std::array<FieldDescriptor, 1> queryParameterFields{ { { "parameter", FieldType::uint8, 1, "enum" } } };

    inline constexpr std::array<FieldDescriptor, 4> algorithmsResponseFields{ {
        { "mode", FieldType::uint8, 1, "enum" },
        { "current", FieldType::uint8, 1, "enum" },
        { "speed", FieldType::uint8, 1, "enum" },
        { "position", FieldType::uint8, 1, "enum" },
    } };

    inline constexpr std::array<FieldDescriptor, 3> parameterResponseFields{ {
        { "parameter", FieldType::uint8, 1, "enum" },
        { "flags", FieldType::uint8, 1, "bitfield" },
        { "value", FieldType::float32, 1, "parameter" },
    } };

    inline constexpr std::array<FieldDescriptor, 2> contractVersionFields{ {
        { "major", FieldType::uint8, 1, "-" },
        { "minor", FieldType::uint8, 1, "-" },
    } };

    inline constexpr std::array<FieldDescriptor, 3> electricalParamsFields{ {
        { "resistance", FieldType::fixed16, focResistanceScale, "ohm" },
        { "inductance", FieldType::fixed16, focInductanceScale, "mH" },
        { "polePairs", FieldType::uint8, 1, "-" },
    } };

    inline constexpr std::array<FieldDescriptor, 4> telemetryCurrentsFields{ {
        { "sequence", FieldType::uint8, 1, "-" },
        { "id", FieldType::fixed16, focStreamCurrentScale, "A" },
        { "iq", FieldType::fixed16, focStreamCurrentScale, "A" },
        { "position", FieldType::fixed16, focStreamPositionScale, "rad" },
    } };

    inline constexpr std::array<FieldDescriptor, 2> categoryErrorFields{ {
        { "originCommandId", FieldType::uint8, 1, "-" },
        { "errorCode", FieldType::uint8, 1, "enum" },
    } };

    inline constexpr std::array<FieldDescriptor, 4> telemetryStatusFields{ {
        { "state", FieldType::uint8, 1, "enum" },
        { "fault", FieldType::uint8, 1, "enum" },
        { "speed", FieldType::fixed16, focSpeedScale, "rad/s" },
        { "position", FieldType::fixed16, focPositionScale, "rad" },
    } };

    constexpr MessageDescriptor MakeDescriptor(uint8_t id, const char* name, Direction direction)
    {
        return MessageDescriptor{ id, name, direction, nullptr, 0 };
    }

    template<std::size_t N>
    constexpr MessageDescriptor MakeDescriptor(uint8_t id, const char* name, Direction direction, const std::array<FieldDescriptor, N>& fields)
    {
        return MessageDescriptor{ id, name, direction, fields.data(), static_cast<uint8_t>(N) };
    }

    inline constexpr std::array<MessageDescriptor, 24> focMotorMessages{ {
        MakeDescriptor(focStartId, "Start", Direction::command),
        MakeDescriptor(focStopId, "Stop", Direction::command),
        MakeDescriptor(focSetPidCurrentId, "SetPidCurrent", Direction::command, bandwidthFields),
        MakeDescriptor(focSetPidSpeedId, "SetPidSpeed", Direction::command, bandwidthFields),
        MakeDescriptor(focSetPidPositionId, "SetPidPosition", Direction::command, bandwidthFields),
        MakeDescriptor(focIdentifyElectricalId, "IdentifyElectrical", Direction::command),
        MakeDescriptor(focRequestTelemetryId, "RequestTelemetry", Direction::command),
        MakeDescriptor(focSetEncoderResolutionId, "SetEncoderResolution", Direction::command, resolutionFields),
        MakeDescriptor(focAlignId, "Align", Direction::command),
        MakeDescriptor(focClearFaultId, "ClearFault", Direction::command),
        MakeDescriptor(focEmergencyStopId, "EmergencyStop", Direction::command),
        MakeDescriptor(focConfigureTelemetryRateId, "ConfigureTelemetryRate", Direction::command, telemetryRateFields),
        MakeDescriptor(focSelectControlModeId, "SelectControlMode", Direction::command, modeFields),
        MakeDescriptor(focSetTorqueSetpointId, "SetTorqueSetpoint", Direction::command, torqueFields),
        MakeDescriptor(focSetSpeedSetpointId, "SetSpeedSetpoint", Direction::command, speedFields),
        MakeDescriptor(focSetPositionSetpointId, "SetPositionSetpoint", Direction::command, positionFields),
        MakeDescriptor(focQueryContractVersionId, "QueryContractVersion", Direction::command),
        MakeDescriptor(focSelectAlgorithmId, "SelectAlgorithm", Direction::command, selectAlgorithmFields),
        MakeDescriptor(focQueryAlgorithmsId, "QueryAlgorithms", Direction::command),
        MakeDescriptor(focSetParameterId, "SetParameter", Direction::command, setParameterFields),
        MakeDescriptor(focQueryParameterId, "QueryParameter", Direction::command, queryParameterFields),
        MakeDescriptor(focCalibrateId, "Calibrate", Direction::command),
        MakeDescriptor(focClearCalibrationId, "ClearCalibration", Direction::command),
        MakeDescriptor(focClearDiagnosticsId, "ClearDiagnostics", Direction::command),
    } };

    inline constexpr std::array<MessageDescriptor, 8> focMotorResponses{ {
        MakeDescriptor(focElectricalParamsResponseId, "ElectricalParamsResponse", Direction::response, electricalParamsFields),
        MakeDescriptor(focTelemetryStatusResponseId, "TelemetryStatusResponse", Direction::telemetry, telemetryStatusFields),
        MakeDescriptor(focTelemetryCurrentsId, "TelemetryCurrents", Direction::telemetry, telemetryCurrentsFields),
        MakeDescriptor(focSelectControlModeResponseId, "SelectControlModeResponse", Direction::response, modeFields),
        MakeDescriptor(focContractVersionResponseId, "ContractVersionResponse", Direction::response, contractVersionFields),
        MakeDescriptor(focAlgorithmsResponseId, "AlgorithmsResponse", Direction::response, algorithmsResponseFields),
        MakeDescriptor(focParameterResponseId, "ParameterResponse", Direction::response, parameterResponseFields),
        MakeDescriptor(services::canCategoryErrorResponseMessageTypeId, "CategoryErrorResponse", Direction::response, categoryErrorFields),
    } };

    constexpr std::optional<MessageDescriptor> FindDescriptor(uint8_t id)
    {
        for (const auto& descriptor : focMotorMessages)
            if (descriptor.id == id)
                return descriptor;

        for (const auto& descriptor : focMotorResponses)
            if (descriptor.id == id)
                return descriptor;

        return std::nullopt;
    }

    inline bool PayloadExact(const hal::Can::Message& data, uint8_t id)
    {
        const auto descriptor = FindDescriptor(id);
        return descriptor.has_value() && data.size() == descriptor->PayloadSize();
    }

    constexpr uint8_t AlgorithmCount(FocControlLoop loop)
    {
        switch (loop)
        {
            case FocControlLoop::current:
                return 4;
            case FocControlLoop::speed:
                return 4;
            case FocControlLoop::position:
                return 5;
            default:
                return 0;
        }
    }

    struct ParameterDescriptor
    {
        FocParameter id;
        const char* name;
        const char* unit;
        float minimum;
        float maximum;
        bool integer;
        bool automatic;
        bool readOnly{ false };
    };

    inline constexpr std::array<uint32_t, 4> focCanBitrates{ 125000, 250000, 500000, 1000000 };
    inline constexpr float focMaxEncoderResolution{ 16777216.0f };
    inline constexpr float focMaxCanNodeId{ 4095.0f };

    inline constexpr float focMaxReading{ std::numeric_limits<float>::max() };

    inline constexpr std::array<ParameterDescriptor, 46> focParameters{ {
        { FocParameter::currentBandwidth, "currentBandwidth", "rad/s", foc::CommandLimits::minBandwidth, foc::CommandLimits::maxCurrentBandwidth, false, true },
        { FocParameter::currentSwitchingGain, "currentSwitchingGain", "A", 0.0f, foc::CommandLimits::maxSwitchingGain, false, false },
        { FocParameter::currentBoundaryLayer, "currentBoundaryLayer", "A", foc::CommandLimits::minBoundaryLayer, foc::CommandLimits::maxBoundaryLayer, false, false },
        { FocParameter::currentTwoStepDeadbeat, "currentTwoStepDeadbeat", "-", 0.0f, 1.0f, true, false },
        { FocParameter::speedBandwidth, "speedBandwidth", "rad/s", foc::CommandLimits::minBandwidth, foc::CommandLimits::maxSpeedBandwidth, false, true },
        { FocParameter::speedErrorWeight, "speedErrorWeight", "-", foc::CommandLimits::minWeight, foc::CommandLimits::maxWeight, false, false },
        { FocParameter::speedIntegralWeight, "speedIntegralWeight", "-", foc::CommandLimits::minWeight, foc::CommandLimits::maxWeight, false, false },
        { FocParameter::speedObserverBandwidthRatio, "speedObserverBandwidthRatio", "-", foc::CommandLimits::minObserverBandwidthRatio, foc::CommandLimits::maxObserverBandwidthRatio, false, false },
        { FocParameter::speedReferenceTimeConstant, "speedReferenceTimeConstant", "s", foc::CommandLimits::minReferenceTimeConstant, foc::CommandLimits::maxReferenceTimeConstant, false, false },
        { FocParameter::positionBandwidth, "positionBandwidth", "rad/s", foc::CommandLimits::minBandwidth, foc::CommandLimits::maxPositionBandwidth, false, true },
        { FocParameter::positionErrorWeight, "positionErrorWeight", "-", foc::CommandLimits::minWeight, foc::CommandLimits::maxWeight, false, false },
        { FocParameter::positionSpeedErrorWeight, "positionSpeedErrorWeight", "-", foc::CommandLimits::minWeight, foc::CommandLimits::maxWeight, false, false },
        { FocParameter::positionIntegralWeight, "positionIntegralWeight", "-", foc::CommandLimits::minWeight, foc::CommandLimits::maxWeight, false, false },
        { FocParameter::positionReferenceTimeConstant, "positionReferenceTimeConstant", "s", foc::CommandLimits::minReferenceTimeConstant, foc::CommandLimits::maxReferenceTimeConstant, false, false },
        { FocParameter::resistance, "resistance", "ohm", foc::CommandLimits::minResistance, foc::CommandLimits::maxResistance, false, false },
        { FocParameter::inductance, "inductance", "mH", foc::CommandLimits::minInductanceMilliHenry, foc::CommandLimits::maxInductanceMilliHenry, false, false },
        { FocParameter::polePairs, "polePairs", "-", 1.0f, foc::CommandLimits::maxPolePairs, true, false },
        { FocParameter::fluxLinkage, "fluxLinkage", "Wb", foc::CommandLimits::minFluxLinkage, foc::CommandLimits::maxFluxLinkage, false, true },
        { FocParameter::inertia, "inertia", "kg.m2", foc::CommandLimits::minInertia, foc::CommandLimits::maxInertia, false, false },
        { FocParameter::viscousFriction, "viscousFriction", "Nm.s/rad", 0.0f, foc::CommandLimits::maxViscousFriction, false, false },
        { FocParameter::encoderResolution, "encoderResolution", "counts/rev", 1.0f, focMaxEncoderResolution, true, false },
        { FocParameter::canNodeId, "canNodeId", "-", 1.0f, focMaxCanNodeId, true, false },
        { FocParameter::canBitrate, "canBitrate", "bit/s", 125000.0f, 1000000.0f, true, false },
        { FocParameter::supervisionWindow, "supervisionWindow", "s", foc::CommandLimits::minSupervisionWindow, foc::CommandLimits::maxSupervisionWindow, false, false },
        { FocParameter::supervisionCurrentFraction, "supervisionCurrentFraction", "-", foc::CommandLimits::minSupervisionCurrentFraction, foc::CommandLimits::maxSupervisionCurrentFraction, false, false },
        { FocParameter::supervisionDemandedSpeed, "supervisionDemandedSpeed", "rad/s", foc::CommandLimits::minSupervisionDemandedSpeed, foc::CommandLimits::maxSupervisionDemandedSpeed, false, false },
        { FocParameter::supervisionPositionError, "supervisionPositionError", "rad", foc::CommandLimits::minSupervisionPositionError, foc::CommandLimits::maxSupervisionPositionError, false, false },
        { FocParameter::supervisionStillExcursion, "supervisionStillExcursion", "rad", foc::CommandLimits::minSupervisionStillExcursion, foc::CommandLimits::maxSupervisionStillExcursion, false, false },
        { FocParameter::stallDetection, "stallDetection", "-", 0.0f, 1.0f, true, false },
        { FocParameter::currentOffsetA, "currentOffsetA", "A", -focMaxReading, focMaxReading, false, false, true },
        { FocParameter::currentOffsetB, "currentOffsetB", "A", -focMaxReading, focMaxReading, false, false, true },
        { FocParameter::currentOffsetC, "currentOffsetC", "A", -focMaxReading, focMaxReading, false, false, true },
        { FocParameter::coggingPeak, "coggingPeak", "Nm", 0.0f, focMaxReading, false, false, true },
        { FocParameter::estimatedResistance, "estimatedResistance", "ohm", 0.0f, focMaxReading, false, false, true },
        { FocParameter::estimatedInductance, "estimatedInductance", "mH", 0.0f, focMaxReading, false, false, true },
        { FocParameter::estimatedInertia, "estimatedInertia", "kg.m2", 0.0f, focMaxReading, false, false, true },
        { FocParameter::estimatedViscousFriction, "estimatedViscousFriction", "Nm.s/rad", 0.0f, focMaxReading, false, false, true },
        { FocParameter::loopSamples, "loopSamples", "-", 0.0f, focMaxReading, true, false, true },
        { FocParameter::loopBudgetCycles, "loopBudgetCycles", "cycles", 0.0f, focMaxReading, true, false, true },
        { FocParameter::loopMinimumCycles, "loopMinimumCycles", "cycles", 0.0f, focMaxReading, true, false, true },
        { FocParameter::loopAverageCycles, "loopAverageCycles", "cycles", 0.0f, focMaxReading, true, false, true },
        { FocParameter::loopMaximumCycles, "loopMaximumCycles", "cycles", 0.0f, focMaxReading, true, false, true },
        { FocParameter::loopOverruns, "loopOverruns", "-", 0.0f, focMaxReading, true, false, true },
        { FocParameter::loopDeadlineMisses, "loopDeadlineMisses", "-", 0.0f, focMaxReading, true, false, true },
        { FocParameter::loopReentries, "loopReentries", "-", 0.0f, focMaxReading, true, false, true },
        { FocParameter::resetCause, "resetCause", "-", 0.0f, 4.0f, true, false, true },
    } };

    constexpr std::optional<ParameterDescriptor> FindParameter(uint8_t id)
    {
        for (const auto& descriptor : focParameters)
            if (static_cast<uint8_t>(descriptor.id) == id)
                return descriptor;

        return std::nullopt;
    }

    constexpr std::optional<ParameterDescriptor> FindParameter(FocParameter parameter)
    {
        return FindParameter(static_cast<uint8_t>(parameter));
    }

    inline bool IsAcceptableParameterValue(const ParameterDescriptor& descriptor, float value)
    {
        if (descriptor.readOnly)
            return false;

        if (descriptor.automatic && value == 0.0f)
            return true;

        if (!foc::IsWithinInclusive(value, descriptor.minimum, descriptor.maximum))
            return false;

        if (descriptor.integer && std::round(value) != value)
            return false;

        if (descriptor.id != FocParameter::canBitrate)
            return true;

        return std::ranges::any_of(focCanBitrates, [value](auto bitrate)
            {
                return static_cast<float>(bitrate) == value;
            });
    }

    inline void WriteFloat32(services::CanPayloadWriter& payload, float value)
    {
        payload.WriteUInt32(std::bit_cast<uint32_t>(value));
    }

    inline float ReadFloat32(services::CanPayloadReader& reader)
    {
        return std::bit_cast<float>(reader.ReadUInt32());
    }
}
