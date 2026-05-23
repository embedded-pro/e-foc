#include "integration_tests/hardware_in_the_loop/support/BridgeConfig.hpp"
#include <stdexcept>

namespace hil
{
    namespace
    {
        BridgeConfig& Storage()
        {
            static BridgeConfig instance;
            return instance;
        }

        bool& IsSet()
        {
            static bool set = false;
            return set;
        }
    }

    void SetBridgeConfig(const BridgeConfig& config)
    {
        Storage() = config;
        IsSet() = true;
    }

    const BridgeConfig& GetBridgeConfig()
    {
        if (!IsSet())
            throw std::runtime_error{ "BridgeConfig not set. Populate it via main() before running scenarios." };
        return Storage();
    }

    BridgeConfig& MutableBridgeConfigForCli()
    {
        IsSet() = true;
        return Storage();
    }
}
