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
    }

    foc::CurrentLoopTunings StoredCurrentTunings(const services::ConfigData& config)
    {
        return foc::CurrentLoopTunings{ config.currentBandwidth, config.currentSwitchingGain, config.currentBoundaryLayer, config.currentTwoStepDeadbeat != 0 };
    }

    foc::SpeedLoopTunings StoredSpeedTunings(const services::ConfigData& config)
    {
        return foc::SpeedLoopTunings{ config.speedBandwidth, config.speedErrorWeight, config.speedIntegralWeight,
            config.speedObserverBandwidthRatio, config.speedReferenceTimeConstant };
    }

    foc::PositionLoopTunings StoredPositionTunings(const services::ConfigData& config)
    {
        return foc::PositionLoopTunings{ config.positionBandwidth, config.positionErrorWeight, config.positionSpeedErrorWeight,
            config.positionIntegralWeight, config.positionReferenceTimeConstant };
    }

    void StoreTunings(services::ConfigData& config, const foc::CurrentLoopTunings& tunings)
    {
        config.currentBandwidth = tunings.bandwidth;
        config.currentSwitchingGain = tunings.switchingGain;
        config.currentBoundaryLayer = tunings.boundaryLayer;
        config.currentTwoStepDeadbeat = tunings.twoStepDeadbeat ? 1 : 0;
    }

    void StoreTunings(services::ConfigData& config, const foc::SpeedLoopTunings& tunings)
    {
        config.speedBandwidth = tunings.bandwidth;
        config.speedErrorWeight = tunings.speedErrorWeight;
        config.speedIntegralWeight = tunings.integralWeight;
        config.speedObserverBandwidthRatio = tunings.observerBandwidthRatio;
        config.speedReferenceTimeConstant = tunings.referenceTimeConstant;
    }

    void StoreTunings(services::ConfigData& config, const foc::PositionLoopTunings& tunings)
    {
        config.positionBandwidth = tunings.bandwidth;
        config.positionErrorWeight = tunings.positionErrorWeight;
        config.positionSpeedErrorWeight = tunings.speedErrorWeight;
        config.positionIntegralWeight = tunings.integralWeight;
        config.positionReferenceTimeConstant = tunings.referenceTimeConstant;
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

    // Inside the boundary layer the sliding-mode error map only contracts while the switching gain stays below it
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
}
