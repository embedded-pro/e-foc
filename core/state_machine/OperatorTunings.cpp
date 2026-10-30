#include "core/state_machine/OperatorTunings.hpp"
#include "core/foc/interfaces/CommandLimits.hpp"
#include "core/foc/math/ParameterValidation.hpp"

namespace state_machine
{
    namespace
    {
        using foc::CommandLimits;

        bool IsAutomaticOr(bool acceptable, float bandwidth)
        {
            return bandwidth == 0.0f || acceptable;
        }

        bool IsAcceptableWeight(float weight)
        {
            return foc::IsWithinInclusive(weight, CommandLimits::minWeight, CommandLimits::maxWeight);
        }

        bool IsAcceptableReferenceTimeConstant(float timeConstant)
        {
            return foc::IsWithinInclusive(timeConstant, CommandLimits::minReferenceTimeConstant, CommandLimits::maxReferenceTimeConstant);
        }

        float WithinOrDefault(float value, float minimum, float maximum, float fallback)
        {
            return foc::IsWithinInclusive(value, minimum, maximum) ? value : fallback;
        }
    }

    foc::CurrentLoopTunings StoredCurrentTunings(const services::ConfigData& config)
    {
        const auto& stored = config.currentLoop;
        return foc::CurrentLoopTunings{ stored.bandwidth, stored.switchingGain, stored.boundaryLayer, config.currentTwoStepDeadbeat != 0 };
    }

    foc::SpeedLoopTunings StoredSpeedTunings(const services::ConfigData& config)
    {
        const auto& stored = config.speedLoop;
        return foc::SpeedLoopTunings{ stored.bandwidth, stored.errorWeight, stored.integralWeight, stored.observerBandwidthRatio, stored.referenceTimeConstant };
    }

    foc::PositionLoopTunings StoredPositionTunings(const services::ConfigData& config)
    {
        const auto& stored = config.positionLoop;
        return foc::PositionLoopTunings{ stored.bandwidth, stored.errorWeight, stored.speedErrorWeight, stored.integralWeight, stored.referenceTimeConstant };
    }

    void StoreTunings(services::ConfigData& config, const foc::CurrentLoopTunings& tunings)
    {
        config.currentLoop = services::CurrentLoopSettings{ tunings.bandwidth, tunings.switchingGain, tunings.boundaryLayer };
        config.currentTwoStepDeadbeat = tunings.twoStepDeadbeat ? 1 : 0;
    }

    void StoreTunings(services::ConfigData& config, const foc::SpeedLoopTunings& tunings)
    {
        config.speedLoop = services::SpeedLoopSettings{ tunings.bandwidth, tunings.speedErrorWeight, tunings.integralWeight, tunings.observerBandwidthRatio, tunings.referenceTimeConstant };
    }

    void StoreTunings(services::ConfigData& config, const foc::PositionLoopTunings& tunings)
    {
        config.positionLoop = services::PositionLoopSettings{ tunings.bandwidth, tunings.positionErrorWeight, tunings.speedErrorWeight, tunings.integralWeight, tunings.referenceTimeConstant };
    }

    bool IsAcceptable(const foc::CurrentLoopTunings& tunings)
    {
        return IsAutomaticOr(foc::IsAcceptableCurrentBandwidth(tunings.bandwidth), tunings.bandwidth) &&
               foc::IsWithinInclusive(tunings.switchingGain, 0.0f, CommandLimits::maxSwitchingGain) &&
               foc::IsWithinInclusive(tunings.boundaryLayer, CommandLimits::minBoundaryLayer, CommandLimits::maxBoundaryLayer);
    }

    bool IsAcceptable(const foc::SpeedLoopTunings& tunings)
    {
        return IsAutomaticOr(foc::IsAcceptableSpeedBandwidth(tunings.bandwidth), tunings.bandwidth) &&
               IsAcceptableWeight(tunings.speedErrorWeight) &&
               IsAcceptableWeight(tunings.integralWeight) &&
               foc::IsWithinInclusive(tunings.observerBandwidthRatio, CommandLimits::minObserverBandwidthRatio, CommandLimits::maxObserverBandwidthRatio) &&
               IsAcceptableReferenceTimeConstant(tunings.referenceTimeConstant);
    }

    bool IsAcceptable(const foc::PositionLoopTunings& tunings)
    {
        return IsAutomaticOr(foc::IsAcceptablePositionBandwidth(tunings.bandwidth), tunings.bandwidth) &&
               IsAcceptableWeight(tunings.positionErrorWeight) &&
               IsAcceptableWeight(tunings.speedErrorWeight) &&
               IsAcceptableWeight(tunings.integralWeight) &&
               IsAcceptableReferenceTimeConstant(tunings.referenceTimeConstant);
    }

    bool IsDesignable(const foc::CurrentLoopTunings& tunings)
    {
        return tunings.switchingGain < tunings.boundaryLayer;
    }

    void SanitizeTunings(services::ConfigData& config)
    {
        const auto defaults = services::ConfigData{};

        if (!IsAcceptable(StoredCurrentTunings(config)) || !IsDesignable(StoredCurrentTunings(config)))
            StoreTunings(config, StoredCurrentTunings(defaults));

        if (!IsAcceptable(StoredSpeedTunings(config)))
            StoreTunings(config, StoredSpeedTunings(defaults));

        if (!IsAcceptable(StoredPositionTunings(config)))
            StoreTunings(config, StoredPositionTunings(defaults));
    }

    void SanitizeSupervision(services::ConfigData& config)
    {
        constexpr services::SupervisionSettings defaults{};
        auto& stored = config.supervision;

        stored.window = WithinOrDefault(stored.window, CommandLimits::minSupervisionWindow, CommandLimits::maxSupervisionWindow, defaults.window);
        stored.currentFraction = WithinOrDefault(stored.currentFraction, CommandLimits::minSupervisionCurrentFraction, CommandLimits::maxSupervisionCurrentFraction, defaults.currentFraction);
        stored.demandedSpeed = WithinOrDefault(stored.demandedSpeed, CommandLimits::minSupervisionDemandedSpeed, CommandLimits::maxSupervisionDemandedSpeed, defaults.demandedSpeed);
        stored.positionError = WithinOrDefault(stored.positionError, CommandLimits::minSupervisionPositionError, CommandLimits::maxSupervisionPositionError, defaults.positionError);
        stored.stillExcursion = WithinOrDefault(stored.stillExcursion, CommandLimits::minSupervisionStillExcursion, CommandLimits::maxSupervisionStillExcursion, defaults.stillExcursion);

        if (config.stallDetection > 1)
            config.stallDetection = services::ConfigData{}.stallDetection;
    }
}
