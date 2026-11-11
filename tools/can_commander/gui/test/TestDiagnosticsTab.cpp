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

        QPushButton& Button(const QString& text)
        {
            for (auto* button : tab.findChildren<QPushButton*>())
                if (button->text() == text)
                    return *button;
            throw std::logic_error("no such button");
        }

        QLabel& Value(const QString& name)
        {
            for (auto* label : tab.findChildren<QLabel*>())
                if (label->accessibleName() == name)
                    return *label;
            throw std::logic_error("no such value");
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
    tab.SetFaultHistoryUnavailable("Connect to a CAN interface to send commands.");

    EXPECT_FALSE(ClearButton().isEnabled());
    EXPECT_EQ("Connect to a CAN interface to send commands.", ClearButton().toolTip());
}

TEST_F(TestDiagnosticsTab, the_model_in_use_and_the_recalibration_counts_are_shown_with_their_units)
{
    EXPECT_EQ("—", Value("Resistance in use").text());

    tab.ShowValue(can::FocParameter::modelResistance, can::FocParameterValue{ 0.42f, false });
    tab.ShowValue(can::FocParameter::recalibrationsApplied, can::FocParameterValue{ 3.0f, false });

    EXPECT_EQ("0.42 \u03A9", Value("Resistance in use").text());
    EXPECT_EQ("3", Value("Recalibrations applied").text());
}

TEST_F(TestDiagnosticsTab, the_flux_linkage_estimate_and_the_flux_linkage_in_use_are_shown_in_webers)
{
    EXPECT_EQ("—", Value("Estimated flux linkage").text());
    EXPECT_EQ("—", Value("Flux linkage in use").text());

    tab.ShowValue(can::FocParameter::estimatedFluxLinkage, can::FocParameterValue{ 0.0042f, false });
    tab.ShowValue(can::FocParameter::modelFluxLinkage, can::FocParameterValue{ 0.0046f, false });

    EXPECT_TRUE(Value("Estimated flux linkage").text().endsWith(" Wb"));
    EXPECT_TRUE(Value("Estimated flux linkage").text().startsWith("0.0042"));
    EXPECT_TRUE(Value("Flux linkage in use").text().startsWith("0.0046"));
}

TEST_F(TestDiagnosticsTab, recalibrate_and_reset_model_ask_the_panel_to_send_them)
{
    int recalibrations{ 0 };
    int resets{ 0 };
    QObject::connect(&tab, &tool::DiagnosticsTab::RecalibrateRequested, [&recalibrations]()
        {
            ++recalibrations;
        });
    QObject::connect(&tab, &tool::DiagnosticsTab::ResetModelRequested, [&resets]()
        {
            ++resets;
        });

    Button("Re&calibrate").click();
    Button("Reset &Model…").click();

    EXPECT_EQ(1, recalibrations);
    EXPECT_EQ(1, resets);
}

TEST_F(TestDiagnosticsTab, unavailable_recalibration_commands_are_disabled_and_say_why)
{
    tab.SetRecalibrationUnavailable("Online recalibration is off: choose a mode on the Configuration tab.", "Stop the motor first.");

    EXPECT_FALSE(Button("Re&calibrate").isEnabled());
    EXPECT_EQ("Online recalibration is off: choose a mode on the Configuration tab.", Button("Re&calibrate").toolTip());
    EXPECT_FALSE(Button("Reset &Model…").isEnabled());
    EXPECT_EQ("Stop the motor first.", Button("Reset &Model…").toolTip());
    EXPECT_EQ("Online recalibration is off: choose a mode on the Configuration tab.", Value("Online recalibration status").text());
}

TEST_F(TestDiagnosticsTab, available_recalibration_commands_are_enabled)
{
    tab.SetRecalibrationUnavailable("Stop the motor first.", "Stop the motor first.");
    tab.SetRecalibrationUnavailable({}, {});

    EXPECT_TRUE(Button("Re&calibrate").isEnabled());
    EXPECT_TRUE(Button("Reset &Model…").isEnabled());
}
