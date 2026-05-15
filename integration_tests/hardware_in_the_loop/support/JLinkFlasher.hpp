#pragma once

#include "integration_tests/hardware_in_the_loop/support/BridgeConfig.hpp"

namespace hil
{
    class JLinkFlasher
    {
    public:
        explicit JLinkFlasher(const BridgeConfig& config);

        void Flash() const;

    private:
        const BridgeConfig& config;
    };
}
