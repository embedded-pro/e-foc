#include "tools/virtual_motor/app/logic/ProfileFile.hpp"
#include "gtest/gtest.h"

namespace
{
    using namespace virtual_motor;

    constexpr const char* example{ "# bench check\n"
                                   "trigger rising 19 50\n"
                                   "\n"
                                   "at 0 load 0.05\n"
                                   "at 0.2 external step 0 0.1 0.05\n"
                                   "at 0.2 current-noise 0.02\n"
                                   "at 1.5 encoder-noise 0.001\n"
                                   "at 2 external none\n" };
}

TEST(TestProfileFile, reads_the_trigger_and_every_kind_of_change)
{
    const auto parsed = app::ParseProfile(example);

    ASSERT_TRUE(parsed.profile.has_value()) << parsed.error;
    const auto& profile = *parsed.profile;
    EXPECT_EQ(profile.trigger.kind, port::TriggerKind::rising);
    EXPECT_EQ(profile.trigger.signal, 18);
    EXPECT_FLOAT_EQ(profile.trigger.level, 50.0f);
    ASSERT_EQ(profile.entries.size(), 5u);
    EXPECT_EQ(profile.entries[0].change, port::MessageType::loadTorque);
    EXPECT_FLOAT_EQ(profile.entries[0].value, 0.05f);
    EXPECT_EQ(profile.entries[1].change, port::MessageType::externalTorque);
    EXPECT_EQ(profile.entries[1].torque.form, port::TorqueForm::step);
    EXPECT_FLOAT_EQ(profile.entries[1].torque.parameters[1], 0.1f);
    EXPECT_FLOAT_EQ(profile.entries[1].torque.parameters[2], 0.05f);
    EXPECT_EQ(profile.entries[2].change, port::MessageType::currentNoise);
    EXPECT_EQ(profile.entries[3].change, port::MessageType::encoderNoise);
    EXPECT_NEAR(profile.entries[3].seconds, 1.5, 1e-12);
    EXPECT_EQ(profile.entries[4].torque.form, port::TorqueForm::none);
}

TEST(TestProfileFile, a_profile_without_a_trigger_starts_immediately)
{
    const auto parsed = app::ParseProfile("at 0 load 0.1\n");

    ASSERT_TRUE(parsed.profile.has_value());
    EXPECT_EQ(parsed.profile->trigger.kind, port::TriggerKind::immediately);
}

TEST(TestProfileFile, reads_the_led_and_protection_triggers)
{
    EXPECT_EQ(app::ParseProfile("trigger leds warning,failure\nat 0 load 0").profile->trigger.leds, port::led::warning + port::led::failure);
    EXPECT_EQ(app::ParseProfile("trigger protection\nat 0 load 0").profile->trigger.kind, port::TriggerKind::protection);
}

TEST(TestProfileFile, an_unreadable_line_is_reported_with_its_number)
{
    EXPECT_EQ(app::ParseProfile("at 0 load 0.1\nat 1 lode 0.2").error, "Line 2: unknown change \"lode\"");
    EXPECT_EQ(app::ParseProfile("at x load 0.1").error, "Line 1: \"x\" is not a time in seconds");
    EXPECT_EQ(app::ParseProfile("at 1 load 0.1\nat 0.5 load 0.2").error, "Line 2: a change cannot come before the one listed ahead of it");
    EXPECT_EQ(app::ParseProfile("at 0 external step 0 1").error, "Line 1: external step takes from, to and delay");
    EXPECT_EQ(app::ParseProfile("trigger rising 26 1").error, "Line 1: the trigger signal is a number from 1 to 25");
    EXPECT_EQ(app::ParseProfile("trigger leds blue").error, "Line 1: unknown LED \"blue\"");
    EXPECT_EQ(app::ParseProfile("at 0 load 0.1\ntrigger protection").error, "Line 2: the trigger comes before the changes");
    EXPECT_EQ(app::ParseProfile("# nothing\n").error, "The profile has no changes");
    EXPECT_EQ(app::ParseProfile("start now").error, "Line 1: a line starts with \"trigger\" or \"at\"");
}

TEST(TestProfileFile, every_malformed_statement_says_what_it_expects)
{
    EXPECT_EQ(app::ParseProfile("at 0 load x").error, "Line 1: \"x\" is not a number");
    EXPECT_EQ(app::ParseProfile("at 0 load").error, "Line 1: load takes one value");
    EXPECT_EQ(app::ParseProfile("at 0").error, "Line 1: a change follows the time");
    EXPECT_EQ(app::ParseProfile("at").error, "Line 1: at is followed by the time in seconds");
    EXPECT_EQ(app::ParseProfile("at -1 load 0").error, "Line 1: \"-1\" is not a time in seconds");
    EXPECT_EQ(app::ParseProfile("at 0 external spin 1").error, "Line 1: external is followed by none, constant, step, ramp or sine");
    EXPECT_EQ(app::ParseProfile("at 0 external none 1").error, "Line 1: external none takes nothing more");
    EXPECT_EQ(app::ParseProfile("trigger").error, "Line 1: trigger is followed by immediately, rising, falling, either, protection or leds");
    EXPECT_EQ(app::ParseProfile("trigger falling 3").error, "Line 1: an edge trigger takes the signal number and the level");
    EXPECT_EQ(app::ParseProfile("trigger either 3 high").error, "Line 1: \"high\" is not a level");
    EXPECT_EQ(app::ParseProfile("trigger leds").error, "Line 1: trigger leds takes the LEDs, separated by commas");
    EXPECT_EQ(app::ParseProfile("trigger protection now").error, "Line 1: trigger protection takes nothing more");
}

TEST(TestProfileFile, reads_windows_line_endings_and_every_external_form)
{
    const auto parsed = app::ParseProfile("trigger immediately\r\nat 0 external constant 0.1\r\nat 1 external ramp 0 0.2 0.5\r\nat 2 external sine 0 0.1 5\r\n");

    ASSERT_TRUE(parsed.profile.has_value()) << parsed.error;
    EXPECT_EQ(parsed.profile->entries[0].torque.form, port::TorqueForm::constant);
    EXPECT_EQ(parsed.profile->entries[1].torque.form, port::TorqueForm::ramp);
    EXPECT_EQ(parsed.profile->entries[2].torque.form, port::TorqueForm::sine);
}

TEST(TestProfileFile, holds_at_most_1024_changes)
{
    std::string text;
    for (int line = 0; line != 1025; ++line)
        text += "at 0 load 0\n";

    EXPECT_EQ(app::ParseProfile(text).error, "Line 1025: a profile holds at most 1024 changes");
}

TEST(TestProfileFile, a_request_times_each_change_in_control_periods)
{
    const auto request = app::ProfileRequestFor(*app::ParseProfile(example).profile, 20000);

    EXPECT_EQ(request.trigger.kind, port::TriggerKind::rising);
    ASSERT_EQ(request.steps.size(), 5u);
    EXPECT_EQ(request.steps[0].at, 0u);
    EXPECT_EQ(request.steps[1].at, 4000u);
    EXPECT_EQ(request.steps[3].at, 30000u);
    EXPECT_EQ(request.steps[1].change, port::MessageType::externalTorque);
}

TEST(TestProfileFile, the_state_reads_in_plain_words)
{
    const auto profile = *app::ParseProfile(example).profile;

    EXPECT_EQ(app::ProfileStatusText(std::nullopt, &profile, 20000), "No profile running");
    EXPECT_EQ(app::ProfileStatusText(port::ProfileState{ port::ProfileStateKind::armed, 0, 5, 0 }, &profile, 20000), "Armed, waiting for the trigger");
    EXPECT_EQ(app::ProfileStatusText(port::ProfileState{ port::ProfileStateKind::running, 3, 5, 25000 }, &profile, 20000),
        "Running: 3 of 5 changes applied, 1.25 s since the trigger; next at 1.5 s: encoder noise 0.001 rad");
    EXPECT_EQ(app::ProfileStatusText(port::ProfileState{ port::ProfileStateKind::running, 3, 5, 25000 }, nullptr, 20000), "Running: 3 of 5 changes applied, 1.25 s since the trigger");
    EXPECT_EQ(app::ProfileStatusText(port::ProfileState{ port::ProfileStateKind::running, 0, 5, 0 }, &profile, 20000), "Running: 0 of 5 changes applied, 0 s since the trigger; next at 0 s: load torque 0.05 N·m");
    EXPECT_EQ(app::ProfileStatusText(port::ProfileState{ port::ProfileStateKind::running, 1, 5, 2000 }, &profile, 0), "Running: 1 of 5 changes applied, 2000 s since the trigger; next at 0.2 s: external torque step from 0.0000 N·m to 0.1000 N·m after 0.050 s");
    EXPECT_EQ(app::ProfileStatusText(port::ProfileState{ port::ProfileStateKind::finished, 5, 5, 40000 }, &profile, 20000), "Finished: 5 changes applied, and they stay in force");
    EXPECT_EQ(app::ProfileStatusText(port::ProfileState{ port::ProfileStateKind::stopped, 2, 5, 100 }, &profile, 20000), "Stopped: the plant is back to its description");
}
