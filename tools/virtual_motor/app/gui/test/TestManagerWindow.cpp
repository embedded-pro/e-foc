#include "tools/virtual_motor/app/gui/AddMotorDialog.hpp"
#include "tools/virtual_motor/app/gui/ManagerWindow.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <QAction>
#include <QDoubleSpinBox>
#include <QMenuBar>
#include <QPushButton>
#include <QTableWidget>
#include <filesystem>

namespace
{
    using namespace virtual_motor::app;

    class FakeProcesses
        : public EngineProcesses
    {
    public:
        std::optional<ProcessId> Start(const std::vector<std::string>&, const std::string&) override
        {
            return ++next;
        }

        void Stop(ProcessId) override
        {}

        ProcessId next{ 0 };
    };

    class FakeStore
        : public RigStore
    {
    public:
        std::vector<MotorSpec> Load() override
        {
            return {};
        }

        void Save(const std::vector<MotorSpec>&) override
        {}
    };

    class TestManagerWindow
        : public testing::Test
    {
    public:
        TestManagerWindow()
        {
            rig.Observe(window);
            rig.Open();
        }

        ~TestManagerWindow() override
        {
            std::filesystem::remove_all(folder);
        }

        QPushButton* Button(const QString& text)
        {
            for (auto* button : window.findChildren<QPushButton*>())
                if (button->text() == text)
                    return button;
            return nullptr;
        }

        QStringList MenuActions()
        {
            QStringList texts;
            for (const auto* action : window.menuBar()->findChildren<QAction*>())
                texts << action->text();
            return texts;
        }

        std::string folder{ (std::filesystem::temp_directory_path() / "e-foc-manager-test").string() };
        FakeProcesses processes;
        FakeStore store;
        Rig rig{ processes, store, folder, 29998 };
        tool::IssueLog issueLog{ tool::IssueLog::Config{ QString::fromStdString(folder + "/logs"), "virtual_motor" } };
        ManagerWindow window{ rig, issueLog, "● hub running on port 29536", {} };
    };
}

TEST_F(TestManagerWindow, each_motor_has_a_row_with_its_node_plant_and_the_state_its_process_is_in)
{
    rig.Add(MotorSpec{ "default", 4, "anaheim" });
    rig.Add(MotorSpec{ "default", 2, "teknic" });
    rig.ProcessEnded(2);

    auto& table = window.Table();
    ASSERT_EQ(table.rowCount(), 2);
    EXPECT_EQ(table.item(0, 0)->text(), "4");
    EXPECT_EQ(table.item(0, 1)->text(), "anaheim");
    EXPECT_EQ(table.item(0, 2)->text(), "Starting…");
    EXPECT_EQ(table.item(0, 3)->text(), "—");
    EXPECT_EQ(table.item(1, 2)->text(), "Crashed");
    EXPECT_THAT(table.item(1, 2)->toolTip().toStdString(), testing::HasSubstr("reports this node offline"));
}

TEST_F(TestManagerWindow, motor_actions_wait_for_a_selection_and_say_so)
{
    rig.Add(MotorSpec{ "default", 4 });

    EXPECT_FALSE(Button("Power Off")->isEnabled());
    EXPECT_EQ(Button("Power Off")->toolTip(), "Select a motor first");

    window.Table().selectRow(0);
    EXPECT_TRUE(Button("Power Off")->isEnabled());
    EXPECT_FALSE(Button("Pause")->isEnabled());
    EXPECT_EQ(Button("Pause")->toolTip(), "Available while the motor runs");
}

TEST_F(TestManagerWindow, without_an_installed_can_commander_the_button_says_why_it_is_disabled)
{
    auto* commander = Button("Open CAN Commander");

    ASSERT_NE(commander, nullptr);
    EXPECT_FALSE(commander->isEnabled());
    EXPECT_THAT(commander->toolTip().toStdString(), testing::HasSubstr("not installed"));
}

TEST_F(TestManagerWindow, with_eight_motors_add_motor_is_disabled_and_says_why)
{
    for (uint8_t node = 1; node != 8; ++node)
        rig.Add(MotorSpec{ "default", node });
    EXPECT_TRUE(Button("Add Motor…")->isEnabled());

    rig.Add(MotorSpec{ "default", 8 });

    EXPECT_FALSE(Button("Add Motor…")->isEnabled());
    EXPECT_THAT(Button("Add Motor…")->toolTip().toStdString(), testing::HasSubstr("at most eight"));
}

TEST_F(TestManagerWindow, what_happens_to_the_motors_is_kept_in_the_issue_log)
{
    rig.Add(MotorSpec{ "default", 4, "anaheim" });
    rig.ProcessEnded(1);

    const auto lines = issueLog.RecentLines(10).join('\n').toStdString();
    EXPECT_THAT(lines, testing::HasSubstr("Node 4 added with the anaheim plant"));
    EXPECT_THAT(lines, testing::HasSubstr("[ERROR] Node 4 crashed"));
}

TEST_F(TestManagerWindow, help_offers_an_issue_report_the_log_folder_and_about)
{
    const auto actions = MenuActions();

    EXPECT_TRUE(actions.contains("&Report Issue…"));
    EXPECT_TRUE(actions.contains("Open &Log Folder"));
    EXPECT_TRUE(actions.contains("&About e-foc Virtual Motor"));
}

TEST(TestAddMotorDialog, a_node_already_in_the_list_cannot_be_added_and_the_dialog_says_why)
{
    AddMotorDialog dialog{ { 1, 2 } };

    dialog.SetNode(2);
    EXPECT_FALSE(dialog.CanAdd());
    EXPECT_EQ(dialog.Problem(), "Node 2 is already in the list; pick another node.");

    dialog.SetNode(3);
    EXPECT_TRUE(dialog.CanAdd());
    EXPECT_EQ(dialog.Spec().node, 3);
    EXPECT_EQ(dialog.Spec().plant, "teknic");
    EXPECT_EQ(dialog.Spec().start, StartMode::keepMemory);
    EXPECT_FLOAT_EQ(dialog.Spec().currentOffset[0], 0.0f);
}

TEST(TestAddMotorDialog, takes_a_current_offset_for_each_phase)
{
    AddMotorDialog dialog{ {} };

    dialog.findChildren<QDoubleSpinBox*>()[1]->setValue(-0.02);

    EXPECT_FLOAT_EQ(dialog.Spec().currentOffset[0], 0.0f);
    EXPECT_FLOAT_EQ(dialog.Spec().currentOffset[1], -0.02f);
    EXPECT_EQ(dialog.findChildren<QDoubleSpinBox*>()[2]->accessibleName(), "Current offset c");
}
