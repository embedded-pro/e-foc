#pragma once

#include "core/services/firmware_upgrade/FirmwareVersion.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tool
{
    struct FirmwareBoard
    {
        uint8_t kind;
        std::string_view name;
        std::string_view microcontroller;
        std::string_view productName;
        bool hasReleasePack;
    };

    enum class PackProblem : uint8_t
    {
        none,
        boardHasNoBootloader,
        tooSmall,
        tooLarge,
        notAPack,
        unknownHeaderVersion,
        anotherTarget,
        notAnApplication
    };

    struct InspectedPack
    {
        std::vector<uint8_t> bytes;
        services::FirmwareVersion version;
        uint32_t crc32{ 0 };
    };

    struct PackInspection
    {
        std::optional<InspectedPack> pack;
        PackProblem problem{ PackProblem::none };
        std::string packBoard;
        std::string packMicrocontroller;
    };

    inline constexpr uint32_t stagingCapacity{ 256 * 1024 };
    inline constexpr uint32_t applicationCapacity{ 256 * 1024 - 64 };

    std::optional<FirmwareBoard> BoardOfKind(uint8_t kind);
    bool CanBeUpdated(const FirmwareBoard& board);
    PackInspection InspectPack(const std::vector<uint8_t>& bytes, const FirmwareBoard& board);
    std::string DescribePackProblem(const PackInspection& inspection, const FirmwareBoard& board);
    std::string VersionText(const services::FirmwareVersion& version);
}
