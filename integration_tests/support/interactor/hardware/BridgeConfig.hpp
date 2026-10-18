#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace hil
{
    struct BridgeConfig
    {
        std::string host;
        uint16_t serialPort{ 0 };
        uint16_t canPort{ 0 };
        uint32_t canBitrate{ 500000 };

        std::chrono::milliseconds connectTimeout{ std::chrono::seconds{ 10 } };

        std::string gdbExe;
        std::string gdbTarget;
        std::string firmwareElf;
        bool skipFlash{ false };
        bool restartPerScenario{ false };

        std::chrono::milliseconds postFlashDelay{ std::chrono::milliseconds{ 500 } };
    };

    void SetBridgeConfig(const BridgeConfig& config);
    const BridgeConfig& GetBridgeConfig();
    BridgeConfig& MutableBridgeConfigForCli();
}
