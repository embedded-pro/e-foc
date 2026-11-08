#include "core/can/FocMotorMessages.hpp"
#include "tools/can_commander/gui/CommandPanel.hpp"
#include "tools/can_commander/logic/SetpointAxis.hpp"
#include "gtest/gtest.h"
#include <QDoubleSpinBox>
#include <QList>

namespace
{
    class TestCommandPanel
        : public testing::Test
    {
    public:
        tool::SetpointKnob& Knob(const char* setpoint)
        {
            for (auto* knob : panel.findChildren<tool::SetpointKnob*>())
                if (knob->accessibleName() == QString("%1 Setpoint knob").arg(setpoint))
                    return *knob;

            ADD_FAILURE() << setpoint;
            return *panel.findChildren<tool::SetpointKnob*>().front();
        }

        QDoubleSpinBox& Spin(const char* setpoint)
        {
            for (auto* spin : panel.findChildren<QDoubleSpinBox*>())
                if (spin->accessibleName() == QString("%1 Setpoint").arg(setpoint))
                    return *spin;

            ADD_FAILURE() << setpoint;
            return *panel.findChildren<QDoubleSpinBox*>().front();
        }

        tool::CommandPanel panel;
    };
}

TEST_F(TestCommandPanel, the_setpoints_offer_the_whole_firmware_limit_in_float_steps)
{
    EXPECT_EQ(tool::TorqueAxis().MaxCounts(), Knob("Torque").maximum());
    EXPECT_EQ(tool::SpeedAxis().MaxCounts(), Knob("Speed").maximum());
    EXPECT_EQ(tool::PositionAxis().MaxCounts(), Knob("Position").maximum());
    EXPECT_EQ(100000, Knob("Torque").maximum());
    EXPECT_NEAR(0.001, Spin("Torque").singleStep(), 1e-9);
    EXPECT_EQ(3, Spin("Torque").decimals());
}
