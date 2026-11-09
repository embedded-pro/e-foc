#include "tools/can_commander/logic/FirmwareIdentityCollector.hpp"
#include <algorithm>
#include <cmath>
#include <span>

namespace tool
{
    namespace
    {
        constexpr std::size_t stagedVersionFirst{ 4 };

        std::optional<std::size_t> IdentitySlot(can::FocParameter parameter)
        {
            const auto slot = std::ranges::find(FirmwareIdentityCollector::parameters, parameter);
            if (slot == FirmwareIdentityCollector::parameters.end())
                return std::nullopt;
            return static_cast<std::size_t>(slot - FirmwareIdentityCollector::parameters.begin());
        }

        uint8_t VersionNumber(const std::optional<float>& value)
        {
            return static_cast<uint8_t>(std::lround(value.value_or(0.0f)));
        }
    }

    bool FirmwareIdentityCollector::IsIdentityParameter(can::FocParameter parameter)
    {
        return IdentitySlot(parameter).has_value();
    }

    void FirmwareIdentityCollector::Begin(uint16_t nodeId)
    {
        identities.erase(nodeId);
    }

    std::optional<FirmwareIdentity> FirmwareIdentityCollector::Collect(uint16_t nodeId, can::FocParameter parameter, float value)
    {
        const auto slot = IdentitySlot(parameter);
        if (!slot.has_value())
            return std::nullopt;

        identities[nodeId].values[*slot] = value;
        return TakeWhenComplete(nodeId);
    }

    std::optional<FirmwareIdentity> FirmwareIdentityCollector::Refused(uint16_t nodeId, can::FocParameter parameter)
    {
        const auto slot = IdentitySlot(parameter);
        if (!slot.has_value() || *slot < stagedVersionFirst)
            return std::nullopt;

        identities[nodeId].stagingRefused = true;
        return TakeWhenComplete(nodeId);
    }

    void FirmwareIdentityCollector::Forget(uint16_t nodeId)
    {
        identities.erase(nodeId);
    }

    std::optional<FirmwareIdentity> FirmwareIdentityCollector::TakeWhenComplete(uint16_t nodeId)
    {
        const auto& partial = identities[nodeId];
        const auto arrived = [&partial](std::size_t first, std::size_t count)
        {
            return std::ranges::all_of(std::span{ partial.values }.subspan(first, count), [](const std::optional<float>& value)
                {
                    return value.has_value();
                });
        };

        const bool staged = arrived(stagedVersionFirst, parameters.size() - stagedVersionFirst);
        if (!arrived(0, stagedVersionFirst) || (!staged && !partial.stagingRefused))
            return std::nullopt;

        const auto& values = partial.values;
        const FirmwareIdentity identity{ { VersionNumber(values[0]), VersionNumber(values[1]), VersionNumber(values[2]) }, VersionNumber(values[3]),
            staged ? services::FirmwareVersion{ VersionNumber(values[4]), VersionNumber(values[5]), VersionNumber(values[6]) } : services::FirmwareVersion{}, staged };
        identities.erase(nodeId);
        return identity;
    }
}
