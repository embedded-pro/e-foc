#include "core/can/FocMotorSettings.hpp"
#include "numerical/math/Math.hpp"
#include <array>
#include <cmath>
#include <optional>

namespace can
{
    static_assert(static_cast<uint8_t>(foc::CurrentAlgorithm::pid) == static_cast<uint8_t>(FocCurrentAlgorithm::pid));
    static_assert(static_cast<uint8_t>(foc::CurrentAlgorithm::decoupledPid) == static_cast<uint8_t>(FocCurrentAlgorithm::decoupledPid));
    static_assert(static_cast<uint8_t>(foc::CurrentAlgorithm::deadbeat) == static_cast<uint8_t>(FocCurrentAlgorithm::deadbeat));
    static_assert(static_cast<uint8_t>(foc::CurrentAlgorithm::slidingMode) == static_cast<uint8_t>(FocCurrentAlgorithm::slidingMode));
    static_assert(static_cast<uint8_t>(foc::SpeedAlgorithm::pid) == static_cast<uint8_t>(FocSpeedAlgorithm::pid));
    static_assert(static_cast<uint8_t>(foc::SpeedAlgorithm::lqi) == static_cast<uint8_t>(FocSpeedAlgorithm::lqi));
    static_assert(static_cast<uint8_t>(foc::SpeedAlgorithm::adrc) == static_cast<uint8_t>(FocSpeedAlgorithm::adrc));
    static_assert(static_cast<uint8_t>(foc::SpeedAlgorithm::twoDof) == static_cast<uint8_t>(FocSpeedAlgorithm::twoDof));
    static_assert(static_cast<uint8_t>(foc::PositionAlgorithm::pid) == static_cast<uint8_t>(FocPositionAlgorithm::pid));
    static_assert(static_cast<uint8_t>(foc::PositionAlgorithm::cascadeP) == static_cast<uint8_t>(FocPositionAlgorithm::cascadeP));
    static_assert(static_cast<uint8_t>(foc::PositionAlgorithm::lqr) == static_cast<uint8_t>(FocPositionAlgorithm::lqr));
    static_assert(static_cast<uint8_t>(foc::PositionAlgorithm::lqi) == static_cast<uint8_t>(FocPositionAlgorithm::lqi));
    static_assert(static_cast<uint8_t>(foc::PositionAlgorithm::twoDof) == static_cast<uint8_t>(FocPositionAlgorithm::twoDof));

    namespace
    {
        struct ReadingRange
        {
            FocParameter first;
            FocParameter last;
            ParameterGroup group;
        };

        // Each group of readings takes a contiguous block of identifiers.
        constexpr std::array<ReadingRange, 4> readingRanges{ {
            { FocParameter::currentOffsetA, FocParameter::coggingPeak, ParameterGroup::calibrationRecord },
            { FocParameter::estimatedResistance, FocParameter::recalibrationsStored, ParameterGroup::onlineEstimate },
            { FocParameter::loopSamples, FocParameter::loopReentries, ParameterGroup::loopStatistics },
            { FocParameter::resetCause, FocParameter::stagedVersionPatch, ParameterGroup::platform },
        } };

        std::optional<ParameterGroup> ReadingGroupOf(FocParameter parameter)
        {
            const auto id = static_cast<uint8_t>(parameter);
            for (const auto& range : readingRanges)
                if (id >= static_cast<uint8_t>(range.first) && id <= static_cast<uint8_t>(range.last))
                    return range.group;

            if (parameter == FocParameter::targetReached || parameter == FocParameter::homed)
                return ParameterGroup::motion;
            if (parameter == FocParameter::windingTemperature)
                return ParameterGroup::thermal;
            return std::nullopt;
        }

        std::optional<ParameterGroup> LoopGroupOf(FocParameter parameter)
        {
            switch (parameter)
            {
                case FocParameter::currentBandwidth:
                case FocParameter::currentSwitchingGain:
                case FocParameter::currentBoundaryLayer:
                case FocParameter::currentTwoStepDeadbeat:
                    return ParameterGroup::currentLoop;
                case FocParameter::speedBandwidth:
                case FocParameter::speedErrorWeight:
                case FocParameter::speedIntegralWeight:
                case FocParameter::speedObserverBandwidthRatio:
                case FocParameter::speedReferenceTimeConstant:
                    return ParameterGroup::speedLoop;
                case FocParameter::positionBandwidth:
                case FocParameter::positionErrorWeight:
                case FocParameter::positionSpeedErrorWeight:
                case FocParameter::positionIntegralWeight:
                case FocParameter::positionReferenceTimeConstant:
                    return ParameterGroup::positionLoop;
                default:
                    return std::nullopt;
            }
        }

        std::optional<ParameterGroup> ConfigurationGroupOf(FocParameter parameter)
        {
            switch (parameter)
            {
                case FocParameter::encoderResolution:
                case FocParameter::canNodeId:
                case FocParameter::canBitrate:
                case FocParameter::telemetryRate:
                    return ParameterGroup::startup;
                case FocParameter::supervisionWindow:
                case FocParameter::supervisionCurrentFraction:
                case FocParameter::supervisionDemandedSpeed:
                case FocParameter::supervisionPositionError:
                case FocParameter::supervisionStillExcursion:
                case FocParameter::stallDetection:
                case FocParameter::recalibrationMode:
                    return ParameterGroup::supervision;
                default:
                    return std::nullopt;
            }
        }
    }

    ParameterGroup GroupOf(FocParameter parameter)
    {
        if (const auto reading = ReadingGroupOf(parameter))
            return *reading;
        if (const auto loop = LoopGroupOf(parameter))
            return *loop;
        if (const auto configuration = ConfigurationGroupOf(parameter))
            return *configuration;

        switch (parameter)
        {
            case FocParameter::overspeedLimit:
            case FocParameter::followingErrorWindow:
            case FocParameter::followingErrorTime:
            case FocParameter::speedAcceleration:
            case FocParameter::speedDeceleration:
            case FocParameter::profileVelocity:
            case FocParameter::profileAcceleration:
            case FocParameter::profileDeceleration:
            case FocParameter::profileJerk:
            case FocParameter::commandTimeout:
            case FocParameter::continuousCurrent:
            case FocParameter::peakCurrent:
            case FocParameter::peakCurrentTime:
            case FocParameter::positionWindow:
            case FocParameter::positionWindowTime:
            case FocParameter::positionMinimum:
            case FocParameter::positionMaximum:
            case FocParameter::homingRequired:
            case FocParameter::calibrationTemperature:
            case FocParameter::temperatureLimit:
            case FocParameter::temperatureMargin:
            case FocParameter::busVoltageMinimum:
            case FocParameter::busVoltageMaximum:
                return ParameterGroup::limits;
            default:
                return ParameterGroup::motor;
        }
    }

    foc::CurrentLoopTunings WithSetting(foc::CurrentLoopTunings tunings, FocParameter parameter, float value)
    {
        if (parameter == FocParameter::currentSwitchingGain)
            tunings.switchingGain = value;
        else if (parameter == FocParameter::currentBoundaryLayer)
            tunings.boundaryLayer = value;
        else if (parameter == FocParameter::currentTwoStepDeadbeat)
            tunings.twoStepDeadbeat = value != 0.0f;
        else
            tunings.bandwidth = value;

        return tunings;
    }

    foc::SpeedLoopTunings WithSetting(foc::SpeedLoopTunings tunings, FocParameter parameter, float value)
    {
        if (parameter == FocParameter::speedErrorWeight)
            tunings.speedErrorWeight = value;
        else if (parameter == FocParameter::speedIntegralWeight)
            tunings.integralWeight = value;
        else if (parameter == FocParameter::speedObserverBandwidthRatio)
            tunings.observerBandwidthRatio = value;
        else if (parameter == FocParameter::speedReferenceTimeConstant)
            tunings.referenceTimeConstant = value;
        else
            tunings.bandwidth = value;

        return tunings;
    }

    foc::PositionLoopTunings WithSetting(foc::PositionLoopTunings tunings, FocParameter parameter, float value)
    {
        if (parameter == FocParameter::positionErrorWeight)
            tunings.positionErrorWeight = value;
        else if (parameter == FocParameter::positionSpeedErrorWeight)
            tunings.speedErrorWeight = value;
        else if (parameter == FocParameter::positionIntegralWeight)
            tunings.integralWeight = value;
        else if (parameter == FocParameter::positionReferenceTimeConstant)
            tunings.referenceTimeConstant = value;
        else
            tunings.bandwidth = value;

        return tunings;
    }

    float SettingOf(const foc::CurrentLoopTunings& tunings, FocParameter parameter)
    {
        if (parameter == FocParameter::currentSwitchingGain)
            return tunings.switchingGain;
        if (parameter == FocParameter::currentBoundaryLayer)
            return tunings.boundaryLayer;
        if (parameter == FocParameter::currentTwoStepDeadbeat)
            return tunings.twoStepDeadbeat ? 1.0f : 0.0f;
        return tunings.bandwidth;
    }

    float SettingOf(const foc::SpeedLoopTunings& tunings, FocParameter parameter)
    {
        if (parameter == FocParameter::speedErrorWeight)
            return tunings.speedErrorWeight;
        if (parameter == FocParameter::speedIntegralWeight)
            return tunings.integralWeight;
        if (parameter == FocParameter::speedObserverBandwidthRatio)
            return tunings.observerBandwidthRatio;
        if (parameter == FocParameter::speedReferenceTimeConstant)
            return tunings.referenceTimeConstant;
        return tunings.bandwidth;
    }

    float SettingOf(const foc::PositionLoopTunings& tunings, FocParameter parameter)
    {
        if (parameter == FocParameter::positionErrorWeight)
            return tunings.positionErrorWeight;
        if (parameter == FocParameter::positionSpeedErrorWeight)
            return tunings.speedErrorWeight;
        if (parameter == FocParameter::positionIntegralWeight)
            return tunings.integralWeight;
        if (parameter == FocParameter::positionReferenceTimeConstant)
            return tunings.referenceTimeConstant;
        return tunings.bandwidth;
    }

    bool IsBandwidth(FocParameter parameter)
    {
        return parameter == FocParameter::currentBandwidth || parameter == FocParameter::speedBandwidth || parameter == FocParameter::positionBandwidth;
    }

    services::MotorParameter ToMotorParameter(FocParameter parameter)
    {
        switch (parameter)
        {
            case FocParameter::inductance:
                return services::MotorParameter::inductance;
            case FocParameter::polePairs:
                return services::MotorParameter::polePairs;
            case FocParameter::fluxLinkage:
                return services::MotorParameter::fluxLinkage;
            case FocParameter::inertia:
                return services::MotorParameter::inertia;
            case FocParameter::viscousFriction:
                return services::MotorParameter::viscousFriction;
            case FocParameter::frictionCoulomb:
                return services::MotorParameter::frictionCoulomb;
            default:
                return services::MotorParameter::resistance;
        }
    }

    uint32_t StartupSettingOf(const services::ConfigData& config, FocParameter parameter)
    {
        if (parameter == FocParameter::canNodeId)
            return config.canNodeId;
        if (parameter == FocParameter::canBitrate)
            return config.canBaudrate;
        if (parameter == FocParameter::telemetryRate)
            return config.telemetryRateHz;
        return config.encoderResolution;
    }

    void WriteStartupSetting(services::ConfigData& config, FocParameter parameter, uint32_t value)
    {
        if (parameter == FocParameter::canNodeId)
            config.canNodeId = value;
        else if (parameter == FocParameter::canBitrate)
            config.canBaudrate = value;
        else if (parameter == FocParameter::telemetryRate)
            config.telemetryRateHz = value;
        else
            config.encoderResolution = value;
    }

    float SupervisionSettingOf(const services::ConfigData& config, FocParameter parameter)
    {
        switch (parameter)
        {
            case FocParameter::supervisionCurrentFraction:
                return config.supervision.currentFraction;
            case FocParameter::supervisionDemandedSpeed:
                return config.supervision.demandedSpeed;
            case FocParameter::supervisionPositionError:
                return config.supervision.positionError;
            case FocParameter::supervisionStillExcursion:
                return config.supervision.stillExcursion;
            case FocParameter::stallDetection:
                return config.stallDetection != 0 ? 1.0f : 0.0f;
            case FocParameter::recalibrationMode:
                return static_cast<float>(config.recalibrationMode);
            default:
                return config.supervision.window;
        }
    }

    void WriteSupervisionSetting(services::ConfigData& config, FocParameter parameter, float value)
    {
        switch (parameter)
        {
            case FocParameter::supervisionCurrentFraction:
                config.supervision.currentFraction = value;
                break;
            case FocParameter::supervisionDemandedSpeed:
                config.supervision.demandedSpeed = value;
                break;
            case FocParameter::supervisionPositionError:
                config.supervision.positionError = value;
                break;
            case FocParameter::supervisionStillExcursion:
                config.supervision.stillExcursion = value;
                break;
            case FocParameter::stallDetection:
                config.stallDetection = value != 0.0f ? 1 : 0;
                break;
            case FocParameter::recalibrationMode:
                config.recalibrationMode = static_cast<uint8_t>(math::Round(value));
                break;
            default:
                config.supervision.window = value;
                break;
        }
    }

    float LimitSettingOf(const services::ConfigData& config, FocParameter parameter)
    {
        switch (parameter)
        {
            case FocParameter::followingErrorWindow:
                return config.limits.followingErrorWindow;
            case FocParameter::followingErrorTime:
                return config.limits.followingErrorTime;
            case FocParameter::speedAcceleration:
                return config.limits.speedAcceleration;
            case FocParameter::speedDeceleration:
                return config.limits.speedDeceleration;
            case FocParameter::profileVelocity:
                return config.limits.profileVelocity;
            case FocParameter::profileAcceleration:
                return config.limits.profileAcceleration;
            case FocParameter::profileDeceleration:
                return config.limits.profileDeceleration;
            case FocParameter::profileJerk:
                return config.limits.profileJerk;
            case FocParameter::commandTimeout:
                return config.limits.commandTimeout;
            case FocParameter::continuousCurrent:
                return config.limits.continuousCurrent;
            case FocParameter::peakCurrent:
                return config.limits.peakCurrent;
            case FocParameter::peakCurrentTime:
                return config.limits.peakCurrentTime;
            case FocParameter::positionWindow:
                return config.limits.positionWindow;
            case FocParameter::positionWindowTime:
                return config.limits.positionWindowTime;
            case FocParameter::positionMinimum:
                return config.limits.positionMinimum;
            case FocParameter::positionMaximum:
                return config.limits.positionMaximum;
            case FocParameter::calibrationTemperature:
                return config.limits.calibrationTemperature;
            case FocParameter::temperatureLimit:
                return config.limits.temperatureLimit;
            case FocParameter::temperatureMargin:
                return config.limits.temperatureMargin;
            case FocParameter::busVoltageMinimum:
                return config.limits.busVoltageMinimum;
            case FocParameter::busVoltageMaximum:
                return config.limits.busVoltageMaximum;
            case FocParameter::homingRequired:
                return config.homingRequired != 0 ? 1.0f : 0.0f;
            default:
                return config.limits.overspeedLimit;
        }
    }

    void WriteLimitSetting(services::ConfigData& config, FocParameter parameter, float value)
    {
        switch (parameter)
        {
            case FocParameter::followingErrorWindow:
                config.limits.followingErrorWindow = value;
                break;
            case FocParameter::followingErrorTime:
                config.limits.followingErrorTime = value;
                break;
            case FocParameter::speedAcceleration:
                config.limits.speedAcceleration = value;
                break;
            case FocParameter::speedDeceleration:
                config.limits.speedDeceleration = value;
                break;
            case FocParameter::profileVelocity:
                config.limits.profileVelocity = value;
                break;
            case FocParameter::profileAcceleration:
                config.limits.profileAcceleration = value;
                break;
            case FocParameter::profileDeceleration:
                config.limits.profileDeceleration = value;
                break;
            case FocParameter::profileJerk:
                config.limits.profileJerk = value;
                break;
            case FocParameter::commandTimeout:
                config.limits.commandTimeout = value;
                break;
            case FocParameter::continuousCurrent:
                config.limits.continuousCurrent = value;
                break;
            case FocParameter::peakCurrent:
                config.limits.peakCurrent = value;
                break;
            case FocParameter::peakCurrentTime:
                config.limits.peakCurrentTime = value;
                break;
            case FocParameter::positionWindow:
                config.limits.positionWindow = value;
                break;
            case FocParameter::positionWindowTime:
                config.limits.positionWindowTime = value;
                break;
            case FocParameter::positionMinimum:
                config.limits.positionMinimum = value;
                break;
            case FocParameter::positionMaximum:
                config.limits.positionMaximum = value;
                break;
            case FocParameter::calibrationTemperature:
                config.limits.calibrationTemperature = value;
                break;
            case FocParameter::temperatureLimit:
                config.limits.temperatureLimit = value;
                break;
            case FocParameter::temperatureMargin:
                config.limits.temperatureMargin = value;
                break;
            case FocParameter::busVoltageMinimum:
                config.limits.busVoltageMinimum = value;
                break;
            case FocParameter::busVoltageMaximum:
                config.limits.busVoltageMaximum = value;
                break;
            case FocParameter::homingRequired:
                config.homingRequired = value != 0.0f ? 1 : 0;
                break;
            default:
                config.limits.overspeedLimit = value;
                break;
        }
    }

    foc::CurrentAlgorithm ToCurrentAlgorithm(uint8_t algorithm)
    {
        return static_cast<foc::CurrentAlgorithm>(algorithm);
    }

    foc::SpeedAlgorithm ToSpeedAlgorithm(uint8_t algorithm)
    {
        return static_cast<foc::SpeedAlgorithm>(algorithm);
    }

    foc::PositionAlgorithm ToPositionAlgorithm(uint8_t algorithm)
    {
        return static_cast<foc::PositionAlgorithm>(algorithm);
    }

    FocCurrentAlgorithm ToCanAlgorithm(foc::CurrentAlgorithm algorithm)
    {
        return static_cast<FocCurrentAlgorithm>(algorithm);
    }

    FocSpeedAlgorithm ToCanAlgorithm(foc::SpeedAlgorithm algorithm)
    {
        return static_cast<FocSpeedAlgorithm>(algorithm);
    }

    FocPositionAlgorithm ToCanAlgorithm(foc::PositionAlgorithm algorithm)
    {
        return static_cast<FocPositionAlgorithm>(algorithm);
    }
}
