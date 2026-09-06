#pragma once

#include <numbers>

namespace foc
{
    struct CommandLimits
    {
        static constexpr float minBandwidth{ 1.0f };
        static constexpr float maxCurrentBandwidth{ 20000.0f };
        static constexpr float maxSpeedBandwidth{ 2000.0f };
        static constexpr float maxPositionBandwidth{ 500.0f };

        static constexpr float maxTorqueSetpoint{ 100.0f };
        static constexpr float maxSpeedSetpoint{ 1000.0f };
        static constexpr float maxPositionSetpoint{ 2.0f * std::numbers::pi_v<float> };

        static constexpr float maxFluxLinkage{ 10.0f };
    };
}
