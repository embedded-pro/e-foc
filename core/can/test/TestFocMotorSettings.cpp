#include "core/can/FocMotorSettings.hpp"
#include "e-foc-protocol/contract/FocMotorWireContract.hpp"
#include <gtest/gtest.h>

TEST(FocMotorSettings, every_loop_setting_is_written_to_and_read_from_its_own_field)
{
    for (const auto& descriptor : can::focParameters)
    {
        const auto value = descriptor.integer ? descriptor.maximum : 0.5f * (descriptor.minimum + descriptor.maximum);

        switch (can::GroupOf(descriptor.id))
        {
            case can::ParameterGroup::currentLoop:
            {
                const auto tunings = can::WithSetting(foc::CurrentLoopTunings{}, descriptor.id, value);
                EXPECT_EQ(can::SettingOf(tunings, descriptor.id), value) << descriptor.name;
                break;
            }
            case can::ParameterGroup::speedLoop:
            {
                const auto tunings = can::WithSetting(foc::SpeedLoopTunings{}, descriptor.id, value);
                EXPECT_EQ(can::SettingOf(tunings, descriptor.id), value) << descriptor.name;
                break;
            }
            case can::ParameterGroup::positionLoop:
            {
                const auto tunings = can::WithSetting(foc::PositionLoopTunings{}, descriptor.id, value);
                EXPECT_EQ(can::SettingOf(tunings, descriptor.id), value) << descriptor.name;
                break;
            }
            default:
                break;
        }
    }
}

TEST(FocMotorSettings, writing_one_loop_setting_leaves_the_others_of_its_loop_alone)
{
    const auto tunings = can::WithSetting(foc::SpeedLoopTunings{}, can::FocParameter::speedObserverBandwidthRatio, 8.0f);

    EXPECT_NEAR(tunings.observerBandwidthRatio, 8.0f, 1e-5f);
    EXPECT_NEAR(tunings.bandwidth, foc::SpeedLoopTunings{}.bandwidth, 1e-6f);
    EXPECT_NEAR(tunings.speedErrorWeight, foc::SpeedLoopTunings{}.speedErrorWeight, 1e-6f);
    EXPECT_NEAR(tunings.integralWeight, foc::SpeedLoopTunings{}.integralWeight, 1e-6f);
    EXPECT_NEAR(tunings.referenceTimeConstant, foc::SpeedLoopTunings{}.referenceTimeConstant, 1e-6f);
}

TEST(FocMotorSettings, every_motor_parameter_reaches_the_calibration_record_field_it_names)
{
    for (const auto& descriptor : can::focParameters)
    {
        if (can::GroupOf(descriptor.id) != can::ParameterGroup::motor)
            continue;

        services::CalibrationData record{};
        const auto value = descriptor.integer ? descriptor.maximum : 0.5f * (descriptor.minimum + descriptor.maximum);
        services::WriteMotorParameter(record, can::ToMotorParameter(descriptor.id), value);

        EXPECT_EQ(services::ReadMotorParameter(record, can::ToMotorParameter(descriptor.id)), value) << descriptor.name;
    }

    services::CalibrationData record{};
    services::WriteMotorParameter(record, can::ToMotorParameter(can::FocParameter::resistance), 0.36f);
    services::WriteMotorParameter(record, can::ToMotorParameter(can::FocParameter::inertia), 7.06e-6f);
    EXPECT_NEAR(record.rPhase, 0.36f, 1e-6f);
    EXPECT_NEAR(record.inertia, 7.06e-6f, 1e-11f);

    services::WriteMotorParameter(record, can::ToMotorParameter(can::FocParameter::frictionCoulomb), 4.0e-3f);
    EXPECT_NEAR(record.frictionCoulomb, 4.0e-3f, 1e-9f);
    EXPECT_EQ(record.frictionViscous, 0.0f);
}

TEST(FocMotorSettings, every_startup_setting_reaches_its_configuration_field)
{
    services::ConfigData config{};

    can::WriteStartupSetting(config, can::FocParameter::encoderResolution, 8192);
    can::WriteStartupSetting(config, can::FocParameter::canNodeId, 12);
    can::WriteStartupSetting(config, can::FocParameter::canBitrate, 500000);

    EXPECT_EQ(config.encoderResolution, 8192u);
    EXPECT_EQ(config.canNodeId, 12u);
    EXPECT_EQ(config.canBaudrate, 500000u);
    EXPECT_EQ(can::StartupSettingOf(config, can::FocParameter::canNodeId), 12u);
}

TEST(FocMotorSettings, every_supervision_setting_reaches_its_configuration_field)
{
    services::ConfigData config{};

    can::WriteSupervisionSetting(config, can::FocParameter::supervisionWindow, 2.0f);
    can::WriteSupervisionSetting(config, can::FocParameter::supervisionCurrentFraction, 0.2f);
    can::WriteSupervisionSetting(config, can::FocParameter::supervisionDemandedSpeed, 5.0f);
    can::WriteSupervisionSetting(config, can::FocParameter::supervisionPositionError, 0.5f);
    can::WriteSupervisionSetting(config, can::FocParameter::supervisionStillExcursion, 0.02f);
    can::WriteSupervisionSetting(config, can::FocParameter::stallDetection, 0.0f);

    EXPECT_NEAR(config.supervision.window, 2.0f, 1e-6f);
    EXPECT_NEAR(config.supervision.currentFraction, 0.2f, 1e-6f);
    EXPECT_NEAR(config.supervision.demandedSpeed, 5.0f, 1e-6f);
    EXPECT_NEAR(config.supervision.positionError, 0.5f, 1e-6f);
    EXPECT_NEAR(config.supervision.stillExcursion, 0.02f, 1e-6f);
    EXPECT_EQ(config.stallDetection, uint8_t{ 0 });
}

TEST(FocMotorSettings, every_limit_reaches_its_configuration_field)
{
    services::ConfigData config{};

    can::WriteLimitSetting(config, can::FocParameter::overspeedLimit, 250.0f);
    can::WriteLimitSetting(config, can::FocParameter::followingErrorWindow, 0.4f);
    can::WriteLimitSetting(config, can::FocParameter::followingErrorTime, 0.25f);
    can::WriteLimitSetting(config, can::FocParameter::speedAcceleration, 300.0f);
    can::WriteLimitSetting(config, can::FocParameter::speedDeceleration, 600.0f);
    can::WriteLimitSetting(config, can::FocParameter::commandTimeout, 0.5f);
    can::WriteLimitSetting(config, can::FocParameter::continuousCurrent, 2.0f);
    can::WriteLimitSetting(config, can::FocParameter::peakCurrent, 3.0f);
    can::WriteLimitSetting(config, can::FocParameter::peakCurrentTime, 2.5f);

    EXPECT_NEAR(config.limits.overspeedLimit, 250.0f, 1e-6f);
    EXPECT_NEAR(config.limits.followingErrorWindow, 0.4f, 1e-6f);
    EXPECT_NEAR(config.limits.followingErrorTime, 0.25f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::followingErrorWindow), 0.4f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::followingErrorTime), 0.25f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::followingErrorWindow), can::ParameterGroup::limits);
    EXPECT_EQ(can::GroupOf(can::FocParameter::followingErrorTime), can::ParameterGroup::limits);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::speedAcceleration), 300.0f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::speedDeceleration), 600.0f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::speedAcceleration), can::ParameterGroup::limits);
    EXPECT_EQ(can::GroupOf(can::FocParameter::speedDeceleration), can::ParameterGroup::limits);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::commandTimeout), 0.5f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::commandTimeout), can::ParameterGroup::limits);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::continuousCurrent), 2.0f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::peakCurrent), 3.0f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::peakCurrentTime), 2.5f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::peakCurrentTime), can::ParameterGroup::limits);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::overspeedLimit), 250.0f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::overspeedLimit), can::ParameterGroup::limits);
}

TEST(FocMotorSettings, the_position_window_and_its_time_are_limits)
{
    services::ConfigData config{};

    can::WriteLimitSetting(config, can::FocParameter::positionWindow, 0.02f);
    can::WriteLimitSetting(config, can::FocParameter::positionWindowTime, 0.05f);

    EXPECT_NEAR(config.limits.positionWindow, 0.02f, 1e-6f);
    EXPECT_NEAR(config.limits.positionWindowTime, 0.05f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::positionWindow), 0.02f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::positionWindowTime), 0.05f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::positionWindow), can::ParameterGroup::limits);
    EXPECT_EQ(can::GroupOf(can::FocParameter::positionWindowTime), can::ParameterGroup::limits);
}

TEST(FocMotorSettings, the_travel_limits_are_limits)
{
    services::ConfigData config{};

    can::WriteLimitSetting(config, can::FocParameter::positionMinimum, -1.5f);
    can::WriteLimitSetting(config, can::FocParameter::positionMaximum, 2.5f);

    EXPECT_NEAR(config.limits.positionMinimum, -1.5f, 1e-6f);
    EXPECT_NEAR(config.limits.positionMaximum, 2.5f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::positionMinimum), -1.5f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::positionMaximum), 2.5f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::positionMinimum), can::ParameterGroup::limits);
    EXPECT_EQ(can::GroupOf(can::FocParameter::positionMaximum), can::ParameterGroup::limits);
}

TEST(FocMotorSettings, the_calibration_temperature_is_a_limit_and_the_winding_temperature_a_thermal_reading)
{
    services::ConfigData config{};

    can::WriteLimitSetting(config, can::FocParameter::calibrationTemperature, 40.0f);

    EXPECT_NEAR(config.limits.calibrationTemperature, 40.0f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::calibrationTemperature), 40.0f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::calibrationTemperature), can::ParameterGroup::limits);
    EXPECT_EQ(can::GroupOf(can::FocParameter::windingTemperature), can::ParameterGroup::thermal);
    const auto descriptor = can::FindParameter(can::FocParameter::windingTemperature);
    ASSERT_TRUE(descriptor.has_value());
    EXPECT_TRUE(descriptor->readOnly);
}

TEST(FocMotorSettings, the_temperature_limit_and_margin_are_limits)
{
    services::ConfigData config{};

    can::WriteLimitSetting(config, can::FocParameter::temperatureLimit, 90.0f);
    can::WriteLimitSetting(config, can::FocParameter::temperatureMargin, 10.0f);

    EXPECT_NEAR(config.limits.temperatureLimit, 90.0f, 1e-6f);
    EXPECT_NEAR(config.limits.temperatureMargin, 10.0f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::temperatureLimit), 90.0f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::temperatureMargin), 10.0f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::temperatureLimit), can::ParameterGroup::limits);
    EXPECT_EQ(can::GroupOf(can::FocParameter::temperatureMargin), can::ParameterGroup::limits);
}

TEST(FocMotorSettings, the_bus_voltage_limits_are_limits)
{
    services::ConfigData config{};

    can::WriteLimitSetting(config, can::FocParameter::busVoltageMinimum, 18.0f);
    can::WriteLimitSetting(config, can::FocParameter::busVoltageMaximum, 32.0f);

    EXPECT_NEAR(config.limits.busVoltageMinimum, 18.0f, 1e-6f);
    EXPECT_NEAR(config.limits.busVoltageMaximum, 32.0f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::busVoltageMinimum), 18.0f, 1e-6f);
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::busVoltageMaximum), 32.0f, 1e-6f);
    EXPECT_EQ(can::GroupOf(can::FocParameter::busVoltageMinimum), can::ParameterGroup::limits);
    EXPECT_EQ(can::GroupOf(can::FocParameter::busVoltageMaximum), can::ParameterGroup::limits);
}

TEST(FocMotorSettings, the_target_reached_flag_is_a_motion_reading)
{
    EXPECT_EQ(can::GroupOf(can::FocParameter::targetReached), can::ParameterGroup::motion);

    const auto descriptor = can::FindParameter(can::FocParameter::targetReached);
    ASSERT_TRUE(descriptor.has_value());
    EXPECT_TRUE(descriptor->readOnly);
    EXPECT_TRUE(descriptor->integer);
}

TEST(FocMotorSettings, every_supervision_setting_reads_back_what_was_written)
{
    for (const auto& descriptor : can::focParameters)
    {
        if (can::GroupOf(descriptor.id) != can::ParameterGroup::supervision)
            continue;

        services::ConfigData config{};
        const auto value = descriptor.integer ? descriptor.minimum : 0.5f * (descriptor.minimum + descriptor.maximum);
        can::WriteSupervisionSetting(config, descriptor.id, value);

        EXPECT_EQ(can::SupervisionSettingOf(config, descriptor.id), value) << descriptor.name;
    }
}

TEST(FocMotorSettings, the_seven_drive_wide_supervision_settings_form_their_own_group)
{
    std::size_t supervision{ 0 };
    for (const auto& descriptor : can::focParameters)
        if (can::GroupOf(descriptor.id) == can::ParameterGroup::supervision)
        {
            ++supervision;
            EXPECT_FALSE(descriptor.automatic) << descriptor.name;
            EXPECT_FALSE(descriptor.readOnly) << descriptor.name;
        }

    EXPECT_EQ(supervision, 7u);
}

TEST(FocMotorSettings, only_the_three_bandwidths_are_bandwidths)
{
    std::size_t bandwidths{ 0 };
    for (const auto& descriptor : can::focParameters)
        if (can::IsBandwidth(descriptor.id))
        {
            ++bandwidths;
            EXPECT_TRUE(descriptor.automatic) << descriptor.name;
        }

    EXPECT_EQ(bandwidths, 3u);
}

TEST(FocMotorSettings, homing_required_is_a_stored_toggle_limit_and_homed_a_read_only_motion_reading)
{
    services::ConfigData config{};

    can::WriteLimitSetting(config, can::FocParameter::homingRequired, 1.0f);

    EXPECT_EQ(config.homingRequired, uint8_t{ 1 });
    EXPECT_NEAR(can::LimitSettingOf(config, can::FocParameter::homingRequired), 1.0f, 1e-6f);
    can::WriteLimitSetting(config, can::FocParameter::homingRequired, 0.0f);
    EXPECT_EQ(config.homingRequired, uint8_t{ 0 });
    EXPECT_EQ(can::GroupOf(can::FocParameter::homingRequired), can::ParameterGroup::limits);
    EXPECT_EQ(can::GroupOf(can::FocParameter::homed), can::ParameterGroup::motion);

    const auto required = can::FindParameter(can::FocParameter::homingRequired);
    ASSERT_TRUE(required.has_value());
    EXPECT_FALSE(required->readOnly);
    EXPECT_TRUE(required->integer);
    const auto homed = can::FindParameter(can::FocParameter::homed);
    ASSERT_TRUE(homed.has_value());
    EXPECT_TRUE(homed->readOnly);
    EXPECT_EQ(static_cast<uint8_t>(can::FocParameter::homed), uint8_t{ 0xC2 });
    EXPECT_EQ(static_cast<uint8_t>(can::FocParameter::homingRequired), uint8_t{ 0x63 });
}
