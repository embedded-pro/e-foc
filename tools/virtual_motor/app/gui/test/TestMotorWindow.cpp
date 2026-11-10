#include "can-lite/drivers/implementation/LoopbackSocket.hpp"
#include "tools/virtual_motor/app/gui/CaptureBar.hpp"
#include "tools/virtual_motor/app/gui/MotorWindow.hpp"
#include "tools/virtual_motor/app/gui/ScopeArea.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <chrono>
#include <filesystem>
#include <thread>

namespace
{
    using namespace virtual_motor;
    using namespace virtual_motor::app;
    namespace loopback = services::loopback;

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

    class TestMotorWindow
        : public testing::Test
    {
    public:
        TestMotorWindow()
        {
            rig.Open();
            rig.Add(MotorSpec{ "default", 3 });
            engine = loopback::Connect(rig.PlantPort().value_or(0));
            Send(port::Hello{ port::protocolVersion, 3, "default", "teknic", 20000 });
            Send(port::Status{ 100, 1.0f, 0, port::RunState::running, 0, 0 });
            PollFor(30);
            window.emplace(rig, 3);
        }

        ~TestMotorWindow() override
        {
            loopback::Close(engine);
            std::filesystem::remove_all(folder);
        }

        template<class Message>
        void Send(const Message& message)
        {
            const auto bytes = port::Encode(message);
            loopback::Send(engine, infra::ConstByteRange{ bytes.data(), bytes.data() + bytes.size() });
        }

        void PollFor(int milliseconds)
        {
            for (int millisecond = 0; millisecond != milliseconds; ++millisecond)
            {
                rig.Poll();
                QApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        template<class Widget>
        Widget* Find(const QString& accessibleName)
        {
            for (auto* widget : window->findChildren<Widget*>())
                if (widget->accessibleName() == accessibleName)
                    return widget;
            return nullptr;
        }

        QPushButton* Button(const QString& text)
        {
            for (auto* button : window->findChildren<QPushButton*>())
                if (button->text() == text)
                    return button;
            return nullptr;
        }

        std::string folder{ (std::filesystem::temp_directory_path() / "e-foc-motor-window-test").string() };
        FakeProcesses processes;
        FakeStore store;
        Rig rig{ processes, store, folder, 29997 };
        intptr_t engine{ loopback::invalidSocket };
        std::optional<MotorWindow> window;
    };
}

TEST_F(TestMotorWindow, the_stream_runs_while_the_window_shows_a_trace_view)
{
    EXPECT_FALSE(rig.Find(3)->Streaming());

    window->show();
    EXPECT_TRUE(rig.Find(3)->Streaming());

    auto* tabs = window->findChild<QTabWidget*>();
    tabs->setCurrentIndex(tabs->count() - 1);
    EXPECT_FALSE(rig.Find(3)->Streaming());

    tabs->setCurrentIndex(0);
    EXPECT_TRUE(rig.Find(3)->Streaming());
    window->hide();
    EXPECT_FALSE(rig.Find(3)->Streaming());
}

TEST_F(TestMotorWindow, the_capture_bar_asks_for_the_trigger_it_shows)
{
    auto* bar = window->findChild<CaptureBar*>();
    Find<QComboBox>("Capture trigger")->setCurrentIndex(1);
    Find<QComboBox>("Trigger signal")->setCurrentIndex(15);
    Find<QDoubleSpinBox>("Trigger level")->setValue(2.5);
    Find<QSpinBox>("Share of the capture before the trigger")->setValue(10);
    Find<QSpinBox>("Capture length")->setValue(1000);

    const auto request = bar->Request();
    EXPECT_EQ(request.trigger.kind, port::TriggerKind::rising);
    EXPECT_EQ(request.trigger.signal, 15);
    EXPECT_NEAR(request.trigger.level, 2.5f, 1e-6f);
    EXPECT_EQ(request.preTriggerPercent, 10);
    EXPECT_EQ(request.length, 1000);
    EXPECT_EQ(Find<QDoubleSpinBox>("Trigger level")->suffix(), " A");
}

TEST_F(TestMotorWindow, an_led_trigger_needs_an_led_before_it_can_be_armed)
{
    Find<QComboBox>("Capture trigger")->setCurrentIndex(5);
    EXPECT_FALSE(Button("Arm")->isEnabled());
    EXPECT_EQ(Button("Arm")->toolTip(), "Choose at least one LED to trigger on");

    for (auto* led : window->findChildren<QCheckBox*>())
        if (led->text() == "Failure")
            led->setChecked(true);
    EXPECT_TRUE(Button("Arm")->isEnabled());
    EXPECT_EQ(window->findChild<CaptureBar*>()->Request().trigger.leds, port::led::failure);
}

TEST_F(TestMotorWindow, a_complete_capture_opens_in_the_view_of_its_trigger_signal)
{
    window->show();
    Find<QComboBox>("Capture trigger")->setCurrentIndex(1);
    Find<QComboBox>("Trigger signal")->setCurrentIndex(18);
    Button("Arm")->click();

    Send(port::CaptureEvent{ 1, port::CaptureEventKind::armed, 100 });
    Send(port::CaptureEvent{ 1, port::CaptureEventKind::triggered, 120 });
    Send(port::CaptureChunk{ 1, 3, 1, 119, 0, { port::Sample{}, port::Sample{}, port::Sample{} } });
    PollFor(50);
    window->RefreshCapture();

    auto* scope = window->findChild<ScopeArea*>();
    EXPECT_EQ(scope->Mode(), ScopeMode::capture);
    ASSERT_NE(scope->CurrentView(), nullptr);
    EXPECT_EQ(scope->CurrentView()->Name(), "Mechanics");
    EXPECT_TRUE(Button("Save CSV…")->isEnabled());

    Button("Back to Live")->click();
    EXPECT_EQ(scope->Mode(), ScopeMode::live);
}

TEST_F(TestMotorWindow, freezing_keeps_the_traces_and_offers_cursors)
{
    window->show();
    Button("Freeze")->click();

    auto* scope = window->findChild<ScopeArea*>();
    EXPECT_EQ(scope->Mode(), ScopeMode::frozen);
    EXPECT_THAT(scope->CurrentView()->CursorSummary(), testing::HasSubstr("left click places A"));
    ASSERT_NE(Button("Run"), nullptr);

    Button("Run")->click();
    EXPECT_EQ(scope->Mode(), ScopeMode::live);
    EXPECT_TRUE(scope->CurrentView()->CursorSummary().empty());
}

TEST_F(TestMotorWindow, the_modulation_rotor_and_encoder_views_follow_the_traces)
{
    window->show();
    auto* tabs = window->findChild<ScopeArea*>()->findChild<QTabWidget*>();

    QStringList titles;
    for (int tab = 0; tab != tabs->count(); ++tab)
        titles << tabs->tabText(tab);
    EXPECT_EQ(titles.join(","), "Currents,Voltages,Mechanics,Supply,Modulation,Rotor,Encoder,Log");

    tabs->setCurrentIndex(5);
    EXPECT_EQ(window->findChild<ScopeArea*>()->CurrentView()->Name(), "Rotor");
    EXPECT_TRUE(rig.Find(3)->Streaming());
}
