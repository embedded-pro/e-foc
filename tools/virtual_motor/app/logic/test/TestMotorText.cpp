#include "tools/virtual_motor/app/logic/MotorText.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using namespace virtual_motor::app;
    namespace port = virtual_motor::port;
}

TEST(TestMotorText, the_real_time_factor_warns_below_098_and_names_the_overruns)
{
    EXPECT_EQ(RealTimeText(port::Status{ 0, 1.0f, 0 }).text, "1.00");
    EXPECT_FALSE(RealTimeText(port::Status{ 0, 1.0f, 0 }).late);

    const auto late = RealTimeText(port::Status{ 0, 0.91f, 7 });
    EXPECT_EQ(late.text, "0.91 (late)");
    EXPECT_TRUE(late.late);
    EXPECT_EQ(late.detail, "7 overruns: the drive is running behind the wall clock");
}

TEST(TestMotorText, the_leds_are_named_not_only_coloured)
{
    EXPECT_EQ(LedText(0), "none lit");
    EXPECT_EQ(LedText(port::led::operational), "operational");
    EXPECT_EQ(LedText(port::led::warning | port::led::failure), "warning, failure");
}

TEST(TestMotorText, the_protection_state_is_spelled_out)
{
    EXPECT_EQ(ProtectionText(0), "clear");
    EXPECT_EQ(ProtectionText(1), "asserted");
    EXPECT_EQ(ProtectionText(2), "unknown");
}

TEST(TestMotorText, virtual_time_reads_as_hours_minutes_seconds_and_milliseconds)
{
    EXPECT_EQ(VirtualTimeText(2046300, 20000), "00:01:42.315");
    EXPECT_EQ(VirtualTimeText(0, 0), "—");
}

TEST(TestMotorText, each_active_change_is_listed_with_its_value_and_unit)
{
    port::Changes changes{ 0.25f, port::ExternalTorque{ port::TorqueForm::sine, { 0.01f, 0.05f, 2.0f } }, 0.02f, 0.001f, port::differs::loadTorque | port::differs::externalTorque | port::differs::currentNoise | port::differs::encoderNoise };

    const auto listed = ActiveChanges(changes);

    ASSERT_EQ(listed.size(), 4u);
    EXPECT_EQ(listed[0].kind, ChangeKind::loadTorque);
    EXPECT_EQ(listed[0].text, "Load torque 0.2500 N·m");
    EXPECT_EQ(listed[1].text, "External torque: sine, offset 0.0100 N·m, amplitude 0.0500 N·m, 2.00 Hz");
    EXPECT_EQ(listed[2].text, "Current noise 0.0200 A");
    EXPECT_EQ(listed[3].text, "Encoder noise 0.00100 rad");
}

TEST(TestMotorText, a_change_back_at_its_description_is_not_listed)
{
    EXPECT_TRUE(ActiveChanges(port::Changes{ 0.25f, {}, 0.02f, 0.001f, 0 }).empty());
}

TEST(TestMotorText, a_log_line_loses_terminal_escape_sequences_and_control_characters)
{
    EXPECT_EQ(PrintableLogLine("1970-01-01T00:00:00.000000 \x1b[2J\x1b[H"), "1970-01-01T00:00:00.000000 ");
    EXPECT_EQ(PrintableLogLine("fsm: \x1b[1;31mfault\x1b[0m\t!"), "fsm: fault !");
    EXPECT_EQ(PrintableLogLine("plain"), "plain");
}

TEST(TestMotorText, what_is_in_force_reads_as_one_line)
{
    const port::Changes changes{ 0.05f, port::ExternalTorque{ port::TorqueForm::constant, { 0.2f } }, 0.02f, 0.001f, 0 };

    EXPECT_EQ(InForceText(changes), "In force: load 0.0500 N·m · external constant 0.2000 N·m · current σ 0.0200 A · encoder σ 0.00100 rad");
}
