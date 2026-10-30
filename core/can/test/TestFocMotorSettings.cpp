#include "core/can/FocMotorSettings.hpp"
#include "core/can/FocMotorWireContract.hpp"
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

TEST(FocMotorSettings, the_six_supervision_settings_form_their_own_group)
{
    std::size_t supervision{ 0 };
    for (const auto& descriptor : can::focParameters)
        if (can::GroupOf(descriptor.id) == can::ParameterGroup::supervision)
        {
            ++supervision;
            EXPECT_FALSE(descriptor.automatic) << descriptor.name;
            EXPECT_FALSE(descriptor.readOnly) << descriptor.name;
        }

    EXPECT_EQ(supervision, 6u);
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
