#pragma once

#include "integration_tests/support/interactor/hardware/BridgeConfig.hpp"

namespace hil
{
    class GdbFlasher
    {
    public:
        explicit GdbFlasher(const BridgeConfig& config);

        void Flash() const;
        void Reset() const;

    private:
        const BridgeConfig& config;
    };
}
