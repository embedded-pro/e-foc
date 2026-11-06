#include "core/can/FocMotorMessages.hpp"
#include "tools/can_commander/gui/DiagnosticsTab.hpp"
#include "gtest/gtest.h"
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <stdexcept>

namespace
{
    class TestDiagnosticsTab
        : public testing::Test
    {
    public:
        QTableWidget& Table()
        {
            return *tab.findChild<QTableWidget*>();
        }

        QLabel& Status()
        {
            for (auto* label : tab.findChildren<QLabel*>())
                if (label->accessibleName() == "Fault history status")
                    return *label;
            throw std::logic_error("no fault history status");
        }

        QPushButton& ClearButton()
        {
            for (auto* button : tab.findChildren<QPushButton*>())
                if (button->text() == "Clear Fault &History")
                    return *button;
            throw std::logic_error("no Clear Fault History button");
        }

        static can::FocFaultRecord Record(uint8_t age, can::FocFaultCode code)
        {
            return can::FocFaultRecord{ age, code, can::FocMotorMode::speed, 1500, 3, foc::Volts{ 41.23f }, foc::Ampere{ -2.5f } };
        }

        tool::DiagnosticsTab tab;
    };
}

TEST_F(TestDiagnosticsTab, a_history_not_yet_read_shows_a_placeholder)
{
    EXPECT_EQ(0, Table().rowCount());
    EXPECT_TRUE(Table().isHidden());
    EXPECT_EQ("—", Status().text());
}

TEST_F(TestDiagnosticsTab, the_records_held_are_listed_newest_first)
{
    tab.ShowFaultRecord(Record(0, can::FocFaultCode::overVoltage));
    tab.ShowFaultRecord(Record(1, can::FocFaultCode::stall));
    tab.ShowFaultRecord(Record(2, can::FocFaultCode::none));

    ASSERT_EQ(2, Table().rowCount());
    EXPECT_FALSE(Table().isHidden());
    EXPECT_TRUE(Status().isHidden());
    EXPECT_EQ("Newest", Table().verticalHeaderItem(0)->text());
    EXPECT_EQ("Over voltage", Table().item(0, 0)->text());
    EXPECT_EQ("Stall", Table().item(1, 0)->text());
    EXPECT_EQ("Speed", Table().item(0, 1)->text());
    EXPECT_EQ("3", Table().item(0, 2)->text());
    EXPECT_EQ("0:00:01.500", Table().item(0, 3)->text());
    EXPECT_EQ("41.23 V", Table().item(0, 4)->text());
    EXPECT_EQ("-2.50 A", Table().item(0, 5)->text());
}

TEST_F(TestDiagnosticsTab, an_empty_history_says_so)
{
    tab.ShowFaultRecord(Record(0, can::FocFaultCode::none));

    EXPECT_EQ(0, Table().rowCount());
    EXPECT_EQ("No faults recorded.", Status().text());
}

TEST_F(TestDiagnosticsTab, a_node_that_leaves_the_bus_takes_its_history_with_it)
{
    tab.ShowFaultRecord(Record(0, can::FocFaultCode::overVoltage));

    tab.ForgetValues();

    EXPECT_EQ(0, Table().rowCount());
    EXPECT_EQ("—", Status().text());
}

TEST_F(TestDiagnosticsTab, an_unavailable_history_cannot_be_cleared_and_says_why)
{
    tab.SetFaultHistoryUnavailable("Needs firmware contract 2.16 or newer: this controller has 2.15.");

    EXPECT_FALSE(ClearButton().isEnabled());
    EXPECT_EQ("Needs firmware contract 2.16 or newer: this controller has 2.15.", ClearButton().toolTip());
}
