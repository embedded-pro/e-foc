#pragma once

#include "core/can/FocMotorMessages.hpp"
#include "core/foc/interfaces/Algorithms.hpp"
#include "core/foc/interfaces/LoopTunings.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"

namespace can
{
    enum class ParameterGroup : uint8_t
    {
        currentLoop,
        speedLoop,
        positionLoop,
        motor,
        startup,
        supervision,
        calibrationRecord,
        onlineEstimate,
        loopStatistics,
        platform
    };

    ParameterGroup GroupOf(FocParameter parameter);

    foc::CurrentLoopTunings WithSetting(foc::CurrentLoopTunings tunings, FocParameter parameter, float value);
    foc::SpeedLoopTunings WithSetting(foc::SpeedLoopTunings tunings, FocParameter parameter, float value);
    foc::PositionLoopTunings WithSetting(foc::PositionLoopTunings tunings, FocParameter parameter, float value);

    float SettingOf(const foc::CurrentLoopTunings& tunings, FocParameter parameter);
    float SettingOf(const foc::SpeedLoopTunings& tunings, FocParameter parameter);
    float SettingOf(const foc::PositionLoopTunings& tunings, FocParameter parameter);

    bool IsBandwidth(FocParameter parameter);

    services::MotorParameter ToMotorParameter(FocParameter parameter);

    uint32_t StartupSettingOf(const services::ConfigData& config, FocParameter parameter);
    void WriteStartupSetting(services::ConfigData& config, FocParameter parameter, uint32_t value);

    float SupervisionSettingOf(const services::ConfigData& config, FocParameter parameter);
    void WriteSupervisionSetting(services::ConfigData& config, FocParameter parameter, float value);

    foc::CurrentAlgorithm ToCurrentAlgorithm(uint8_t algorithm);
    foc::SpeedAlgorithm ToSpeedAlgorithm(uint8_t algorithm);
    foc::PositionAlgorithm ToPositionAlgorithm(uint8_t algorithm);
    FocCurrentAlgorithm ToCanAlgorithm(foc::CurrentAlgorithm algorithm);
    FocSpeedAlgorithm ToCanAlgorithm(foc::SpeedAlgorithm algorithm);
    FocPositionAlgorithm ToCanAlgorithm(foc::PositionAlgorithm algorithm);
}
