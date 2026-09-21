#pragma once

#include "core/can/FocMotorMessages.hpp"
#include "hal/interfaces/Can.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace can
{
    static constexpr uint8_t focContractVersionMajor = 1;
    static constexpr uint8_t focContractVersionMinor = 0;

    enum class FieldType : uint8_t
    {
        uint8,
        fixed16,
        uint32
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

    inline constexpr std::array<FieldDescriptor, 2> contractVersionFields{ {
        { "major", FieldType::uint8, 1, "-" },
        { "minor", FieldType::uint8, 1, "-" },
    } };

    inline constexpr std::array<FieldDescriptor, 3> electricalParamsFields{ {
        { "resistance", FieldType::fixed16, focResistanceScale, "ohm" },
        { "inductance", FieldType::fixed16, focInductanceScale, "H" },
        { "polePairs", FieldType::uint8, 1, "-" },
    } };

    inline constexpr std::array<FieldDescriptor, 2> mechanicalParamsFields{ {
        { "friction", FieldType::fixed16, focFrictionScale, "Nm.s/rad" },
        { "inertia", FieldType::fixed16, focInertiaScale, "kg.m2" },
    } };

    inline constexpr std::array<FieldDescriptor, 4> telemetryElectricalFields{ {
        { "busVoltage", FieldType::fixed16, focVoltageScale, "V" },
        { "maxCurrent", FieldType::fixed16, focCurrentScale, "A" },
        { "iq", FieldType::fixed16, focCurrentScale, "A" },
        { "id", FieldType::fixed16, focCurrentScale, "A" },
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

    inline constexpr std::array<MessageDescriptor, 19> focMotorMessages{ {
        MakeDescriptor(focQueryMotorTypeId, "QueryMotorType", Direction::command),
        MakeDescriptor(focStartId, "Start", Direction::command),
        MakeDescriptor(focStopId, "Stop", Direction::command),
        MakeDescriptor(focSetPidCurrentId, "SetPidCurrent", Direction::command, bandwidthFields),
        MakeDescriptor(focSetPidSpeedId, "SetPidSpeed", Direction::command, bandwidthFields),
        MakeDescriptor(focSetPidPositionId, "SetPidPosition", Direction::command, bandwidthFields),
        MakeDescriptor(focIdentifyElectricalId, "IdentifyElectrical", Direction::command),
        MakeDescriptor(focIdentifyMechanicalId, "IdentifyMechanical", Direction::command),
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
    } };

    inline constexpr std::array<MessageDescriptor, 7> focMotorResponses{ {
        MakeDescriptor(focMotorTypeResponseId, "MotorTypeResponse", Direction::response, modeFields),
        MakeDescriptor(focElectricalParamsResponseId, "ElectricalParamsResponse", Direction::response, electricalParamsFields),
        MakeDescriptor(focMechanicalParamsResponseId, "MechanicalParamsResponse", Direction::response, mechanicalParamsFields),
        MakeDescriptor(focTelemetryElectricalResponseId, "TelemetryElectricalResponse", Direction::telemetry, telemetryElectricalFields),
        MakeDescriptor(focTelemetryStatusResponseId, "TelemetryStatusResponse", Direction::telemetry, telemetryStatusFields),
        MakeDescriptor(focSelectControlModeResponseId, "SelectControlModeResponse", Direction::response, modeFields),
        MakeDescriptor(focContractVersionResponseId, "ContractVersionResponse", Direction::response, contractVersionFields),
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

    constexpr uint8_t PayloadSizeOf(uint8_t id)
    {
        const auto descriptor = FindDescriptor(id);
        return descriptor.has_value() ? descriptor->PayloadSize() : 0;
    }

    inline bool PayloadExact(const hal::Can::Message& data, uint8_t id)
    {
        const auto descriptor = FindDescriptor(id);
        return descriptor.has_value() && data.size() == descriptor->PayloadSize();
    }
}
