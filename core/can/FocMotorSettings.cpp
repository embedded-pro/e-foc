#include "core/can/FocMotorSettings.hpp"

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

    ParameterGroup GroupOf(FocParameter parameter)
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
            case FocParameter::encoderResolution:
            case FocParameter::canNodeId:
            case FocParameter::canBitrate:
                return ParameterGroup::startup;
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
        return config.encoderResolution;
    }

    void WriteStartupSetting(services::ConfigData& config, FocParameter parameter, uint32_t value)
    {
        if (parameter == FocParameter::canNodeId)
            config.canNodeId = value;
        else if (parameter == FocParameter::canBitrate)
            config.canBaudrate = value;
        else
            config.encoderResolution = value;
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
