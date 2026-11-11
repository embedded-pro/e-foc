#include "e-foc-protocol/contract/FocMotorMessages.hpp"
#include "tools/can_commander/gui/SettingEditor.hpp"
#include "tools/can_commander/logic/MotorSettings.hpp"
#include "gtest/gtest.h"
#include <QComboBox>

namespace
{
    class TestSettingEditor
        : public testing::Test
    {
    public:
        QComboBox& Choice()
        {
            return *editor.findChild<QComboBox*>();
        }

        tool::SettingEditor editor{ tool::PresentationOf(can::FocParameter::recalibrationMode) };
    };
}

TEST_F(TestSettingEditor, the_recalibration_mode_is_chosen_by_name)
{
    ASSERT_EQ(3, Choice().count());
    EXPECT_EQ("Off", Choice().itemText(0));
    EXPECT_EQ("On command", Choice().itemText(1));
    EXPECT_EQ("Automatic", Choice().itemText(2));
}

TEST_F(TestSettingEditor, the_held_recalibration_mode_is_selected_and_an_other_one_is_an_edit)
{
    editor.SetHeld(can::FocParameterValue{ 2.0f, false });
    EXPECT_EQ("Automatic", Choice().currentText());
    EXPECT_FALSE(editor.IsEdited());

    Choice().setCurrentIndex(1);

    EXPECT_TRUE(editor.IsEdited());
    EXPECT_EQ(1.0f, editor.EditedValue());
    EXPECT_EQ("Online recalibration On command", editor.Describe(1.0f));
}
