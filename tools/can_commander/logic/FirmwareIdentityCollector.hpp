#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareVersion.hpp"
#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <optional>

namespace tool
{
    struct FirmwareIdentity
    {
        services::FirmwareVersion running;
        uint8_t boardKind{ 0 };
        services::FirmwareVersion staged;
        bool hasUpgradeService{ true };

        bool operator==(const FirmwareIdentity& other) const = default;
    };

    // Assembles a node's identity from its parameter replies; a node without an upgrade service refuses the staged-version reads.
    class FirmwareIdentityCollector
    {
    public:
        static constexpr std::array<can::FocParameter, 7> parameters{
            can::FocParameter::firmwareVersionMajor,
            can::FocParameter::firmwareVersionMinor,
            can::FocParameter::firmwareVersionPatch,
            can::FocParameter::boardKind,
            can::FocParameter::stagedVersionMajor,
            can::FocParameter::stagedVersionMinor,
            can::FocParameter::stagedVersionPatch,
        };

        static bool IsIdentityParameter(can::FocParameter parameter);

        void Begin(uint16_t nodeId);
        std::optional<FirmwareIdentity> Collect(uint16_t nodeId, can::FocParameter parameter, float value);
        std::optional<FirmwareIdentity> Refused(uint16_t nodeId, can::FocParameter parameter);
        void Forget(uint16_t nodeId);

    private:
        struct PartialIdentity
        {
            std::array<std::optional<float>, parameters.size()> values;
            bool stagingRefused{ false };
        };

        std::optional<FirmwareIdentity> TakeWhenComplete(uint16_t nodeId);

        std::map<uint16_t, PartialIdentity> identities;
    };
}
