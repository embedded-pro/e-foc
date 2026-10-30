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

        static constexpr float maxSwitchingGain{ 5.0f };
        static constexpr float minBoundaryLayer{ 0.01f };
        static constexpr float maxBoundaryLayer{ 5.0f };
        static constexpr float minWeight{ 1.0e-3f };
        static constexpr float maxWeight{ 1.0e3f };
        static constexpr float minObserverBandwidthRatio{ 1.0f };
        static constexpr float maxObserverBandwidthRatio{ 20.0f };
        static constexpr float minReferenceTimeConstant{ 1.0e-4f };
        static constexpr float maxReferenceTimeConstant{ 10.0f };

        static constexpr float minResistance{ 1.0e-4f };
        static constexpr float maxResistance{ 100.0f };
        static constexpr float minInductanceMilliHenry{ 1.0e-4f };
        static constexpr float maxInductanceMilliHenry{ 1000.0f };
        static constexpr float maxPolePairs{ 64.0f };
        static constexpr float minFluxLinkage{ 1.0e-5f };
        static constexpr float maxFluxLinkage{ 10.0f };
        static constexpr float minInertia{ 1.0e-9f };
        static constexpr float maxInertia{ 10.0f };
        static constexpr float maxViscousFriction{ 10.0f };

        static constexpr float minSupervisionWindow{ 0.05f };
        static constexpr float maxSupervisionWindow{ 10.0f };
        static constexpr float minSupervisionCurrentFraction{ 0.01f };
        static constexpr float maxSupervisionCurrentFraction{ 1.0f };
        static constexpr float minSupervisionDemandedSpeed{ 0.1f };
        static constexpr float maxSupervisionDemandedSpeed{ 500.0f };
        static constexpr float minSupervisionPositionError{ 0.01f };
        static constexpr float maxSupervisionPositionError{ 3.2f };
        static constexpr float minSupervisionStillExcursion{ 1.0e-3f };
        static constexpr float maxSupervisionStillExcursion{ 1.0f };
    };
}
