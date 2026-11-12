#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "tools/can_commander/gui/ConfigurationTab.hpp"
#include "gtest/gtest.h"
#include <QGroupBox>
#include <QLineEdit>
#include <algorithm>

namespace
{
    class TestConfigurationTab
        : public testing::Test
    {
    public:
        QLineEdit& RateEntry()
        {
            for (auto* entry : tab.findChildren<QLineEdit*>())
                if (entry->accessibleName() == "Current stream rate")
                    return *entry;

            ADD_FAILURE() << "no current stream rate entry";
            return *tab.findChildren<QLineEdit*>().front();
        }

        tool::ConfigurationTab tab;
    };
}

TEST_F(TestConfigurationTab, the_current_stream_rate_has_a_group_of_its_own)
{
    const auto groups = tab.findChildren<QGroupBox*>();
    EXPECT_TRUE(std::any_of(groups.begin(), groups.end(), [](const QGroupBox* group)
        {
            return group->title() == "Current Stream";
        }));
    EXPECT_NE(nullptr, tab.EditorFor(can::FocParameter::telemetryRate));
}

TEST_F(TestConfigurationTab, the_current_stream_rate_shows_what_the_controller_holds)
{
    tab.ShowValue(can::FocParameter::telemetryRate, can::FocParameterValue{ 250.0f, false });

    EXPECT_EQ("250", RateEntry().text());
    EXPECT_FALSE(tab.EditorFor(can::FocParameter::telemetryRate)->IsEdited());
}

TEST_F(TestConfigurationTab, a_current_stream_rate_above_one_kilohertz_is_refused_before_it_is_sent)
{
    tab.ShowValue(can::FocParameter::telemetryRate, can::FocParameterValue{ 100.0f, false });

    RateEntry().setText("1001");

    const auto* editor = tab.EditorFor(can::FocParameter::telemetryRate);
    EXPECT_FALSE(editor->IsValid());
    EXPECT_EQ("Current stream rate: enter a whole number from 0 to 1000 Hz", editor->Problem());
}
