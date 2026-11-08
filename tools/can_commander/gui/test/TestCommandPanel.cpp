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

TEST_F(TestCommandPanel, the_setpoints_start_on_the_wire_count_of_the_scaled_commands)
{
    EXPECT_FALSE(panel.PreciseSetpoints());

    EXPECT_EQ(tool::TorqueAxis().MaxCounts(), Knob("Torque").maximum());
    EXPECT_EQ(tool::SpeedAxis().MaxCounts(), Knob("Speed").maximum());
    EXPECT_EQ(tool::PositionAxis().MaxCounts(), Knob("Position").maximum());
}

TEST_F(TestCommandPanel, a_precise_node_gets_the_finer_steps)
{
    panel.SetPreciseSetpoints(true);

    EXPECT_EQ(tool::PreciseAxisFor(can::FocMotorMode::torque).MaxCounts(), Knob("Torque").maximum());
    EXPECT_EQ(tool::PreciseAxisFor(can::FocMotorMode::speed).MaxCounts(), Knob("Speed").maximum());
    EXPECT_EQ(tool::PreciseAxisFor(can::FocMotorMode::position).MaxCounts(), Knob("Position").maximum());
    EXPECT_EQ(100000, Knob("Torque").maximum());
    EXPECT_NEAR(0.001, Spin("Torque").singleStep(), 1e-9);
    EXPECT_EQ(3, Spin("Torque").decimals());
}

TEST_F(TestCommandPanel, going_back_to_an_older_node_restores_the_scaled_steps_and_forgets_the_accepted_values)
{
    panel.SetPreciseSetpoints(true);
    panel.ShowAcceptedSetpoint(can::FocMotorMode::speed, 250);
    panel.SetPreciseSetpoints(false);

    EXPECT_EQ(tool::SpeedAxis().MaxCounts(), Knob("Speed").maximum());
    EXPECT_EQ(0, Knob("Speed").value());
}
