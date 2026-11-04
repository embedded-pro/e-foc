#pragma once

#include "can-lite/categories/firmware_upgrade/FirmwareUpgradeDefinitions.hpp"
#include "integration_tests/support/Fixture.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace integration
{
    class FirmwareUpgradeClient
    {
    public:
        struct BeginAnswer
        {
            services::FwuError error;
            uint16_t pageSize;
        };

        explicit FirmwareUpgradeClient(Fixture& fixture);

        std::optional<BeginAnswer> Begin(uint32_t size);
        std::optional<services::FwuError> Block(const std::vector<uint8_t>& pack, uint16_t index);
        std::optional<services::FwuError> Blocks(const std::vector<uint8_t>& pack, uint16_t first, uint16_t count);
        std::optional<services::FwuError> Verify(uint32_t crc);
        std::optional<services::FwuError> Activate();
        bool Abort();

        static uint16_t NumberOfBlocks(const std::vector<uint8_t>& pack);
        static uint32_t Crc(const std::vector<uint8_t>& pack);

    private:
        std::optional<hal::Can::Message> Exchange(uint8_t command, const hal::Can::Message& payload, uint8_t response, std::chrono::milliseconds timeout);
        std::optional<services::FwuError> StatusOf(const std::optional<hal::Can::Message>& answer) const;
        void KeepClientAlive();

        Fixture& fixture;
        std::chrono::steady_clock::time_point lastHeartbeat{};
    };
}
