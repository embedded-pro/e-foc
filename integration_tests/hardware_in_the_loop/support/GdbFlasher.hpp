#pragma once

#include "integration_tests/hardware_in_the_loop/support/BridgeConfig.hpp"

namespace hil
{
    class GdbFlasher
    {
    public:
        explicit GdbFlasher(const BridgeConfig& config);

        void Flash() const;

    private:
        const BridgeConfig& config;
    };
}
