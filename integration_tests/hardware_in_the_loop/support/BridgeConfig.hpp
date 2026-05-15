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

        std::chrono::milliseconds connectTimeout{ std::chrono::seconds{ 10 } };

        std::string jlinkExe;
        std::string jlinkDevice;
        std::string jlinkHex;
        std::string jlinkInterface;
        uint32_t jlinkSpeedKHz{ 0 };
        bool skipFlash{ false };

        std::chrono::milliseconds postFlashDelay{ std::chrono::milliseconds{ 500 } };
    };

    void SetBridgeConfig(const BridgeConfig& config);
    const BridgeConfig& GetBridgeConfig();
    BridgeConfig& MutableBridgeConfigForCli();
}
