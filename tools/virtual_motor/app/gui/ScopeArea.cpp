#include "tools/virtual_motor/app/gui/ScopeArea.hpp"
#include "tools/virtual_motor/app/logic/CaptureCsv.hpp"
#include "ui/backend/qt/QtPaintedWidget.hpp"
#include "ui/backend/qt/QtTheme.hpp"
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <array>

namespace virtual_motor::app
{
    namespace
    {
        constexpr int repaintIntervalMs{ 40 };
        constexpr uint32_t defaultControlRateHz{ 20000 };

        struct SpanChoice
        {
            const char* text;
            double seconds;
        };

        constexpr std::array<SpanChoice, 10> spans{ {
            { "10 ms", 0.01 },
            { "20 ms", 0.02 },
            { "50 ms", 0.05 },
            { "100 ms", 0.1 },
            { "200 ms", 0.2 },
            { "500 ms", 0.5 },
            { "1 s", 1.0 },
            { "2 s", 2.0 },
            { "5 s", 5.0 },
            { "10 s", 10.0 },
        } };

        constexpr int defaultSpan{ 4 };
    }

    ScopeArea::ScopeArea(RigMotor& motor, QWidget* parent)
        : QWidget(parent)
        , motor(motor)
    {
        tabs = new QTabWidget(this);
        for (const auto& spec : TraceViews())
        {
            auto& view = *views.emplace_back(std::make_unique<TraceView>(spec));
            auto* widget = new ui::backend::qt::QtPaintedWidget(view, tabs);
            widget->SetBackgroundRole(ui::theme::ColorRole::ScopeBackground);
            const auto name = QString::fromUtf8(spec.name.data(), static_cast<qsizetype>(spec.name.size()));
            widget->setAccessibleName(name + " traces");
            tabs->addTab(widget, name);
            view.OnCursorsChanged([this]()
                {
                    RefreshToolbar();
                });
        }

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(BuildToolbar());
        layout->addWidget(tabs, 1);
        ConnectToolbar();

        timer = new QTimer(this);
        timer->setInterval(repaintIntervalMs);
        connect(timer, &QTimer::timeout, this, &ScopeArea::Tick);
        timer->start();
        SetMode(ScopeMode::live);
    }

    void ScopeArea::AddPage(QWidget* page, const QString& title)
    {
        tabs->addTab(page, title);
    }

    bool ScopeArea::ShowsTrace() const
    {
        return CurrentView() != nullptr;
    }

    ScopeMode ScopeArea::Mode() const
    {
        return mode;
    }

    TraceView* ScopeArea::CurrentView() const
    {
        const auto index = tabs->currentIndex();
        return index >= 0 && static_cast<std::size_t>(index) < views.size() ? views[static_cast<std::size_t>(index)].get() : nullptr;
    }

    void ScopeArea::ShowCapture(std::optional<uint8_t> signal)
    {
        if (!motor.Captures().Last())
            return;
        if (signal)
            if (const auto view = ViewOfSignal(*signal))
                tabs->setCurrentIndex(static_cast<int>(*view));
        if (!ShowsTrace())
            tabs->setCurrentIndex(0);
        SetMode(ScopeMode::capture);
    }

    void ScopeArea::Refresh()
    {
        RefreshToolbar();
    }

    QWidget* ScopeArea::BuildToolbar()
    {
        auto* toolbar = new QWidget(this);
        auto* layout = new QHBoxLayout(toolbar);
        layout->setContentsMargins(0, 0, 0, 0);
        span = new QComboBox(toolbar);
        span->setAccessibleName("Time span");
        span->setToolTip("Time span of the live traces");
        for (const auto& choice : spans)
            span->addItem(choice.text);
        span->setCurrentIndex(defaultSpan);
        freeze = new QPushButton("Freeze", toolbar);
        live = new QPushButton("Back to Live", toolbar);
        live->setToolTip("Show the live traces again");
        save = new QPushButton("Save CSV…", toolbar);
        save->setToolTip("Save the capture shown, every signal at every control period");
        modeLabel = new QLabel(toolbar);
        cursors = new QLabel(toolbar);
        for (auto* widget : std::initializer_list<QWidget*>{ span, freeze, live, save, modeLabel })
            layout->addWidget(widget);
        layout->addStretch(1);
        layout->addWidget(cursors);
        return toolbar;
    }

    void ScopeArea::ConnectToolbar()
    {
        connect(span, &QComboBox::currentIndexChanged, this, [this]()
            {
                stale = true;
            });
        connect(freeze, &QPushButton::clicked, this, &ScopeArea::ToggleFreeze);
        connect(live, &QPushButton::clicked, this, [this]()
            {
                SetMode(ScopeMode::live);
            });
        connect(save, &QPushButton::clicked, this, &ScopeArea::SaveCapture);
        connect(tabs, &QTabWidget::currentChanged, this, [this]()
            {
                stale = true;
                RefreshToolbar();
                emit TraceShownChanged();
            });
    }

    void ScopeArea::SetMode(ScopeMode newMode)
    {
        mode = newMode;
        if (mode != ScopeMode::frozen)
        {
            frozen.reset();
            frozenTrace.reset();
        }
        for (const auto& view : views)
            view->SetCursorsEnabled(mode != ScopeMode::live);
        stale = true;
        RefreshToolbar();
        Tick();
    }

    void ScopeArea::ToggleFreeze()
    {
        if (mode == ScopeMode::frozen)
        {
            SetMode(ScopeMode::live);
            return;
        }
        frozen.emplace(motor.History());
        frozenTrace.emplace(*frozen, ControlRate());
        SetMode(ScopeMode::frozen);
    }

    void ScopeArea::SaveCapture()
    {
        const auto& capture = motor.Captures().Last();
        if (!capture)
            return;
        const auto suggested = QString("node%1-capture%2.csv").arg(motor.Spec().node).arg(capture->number);
        const auto path = QFileDialog::getSaveFileName(this, "Save Capture", suggested, "Comma-separated values (*.csv)");
        if (path.isEmpty())
            return;
        QFile file{ path };
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(QByteArray::fromStdString(CaptureCsv(*capture, ControlRate()))) < 0)
            QMessageBox::warning(this, "Save Capture", "The capture could not be written to " + path + ": " + file.errorString());
    }

    void ScopeArea::Tick()
    {
        auto* view = CurrentView();
        if (view == nullptr || !isVisible())
            return;
        const bool moved = mode == ScopeMode::live && motor.History().Revision() != shownRevision;
        if (!stale && !moved && view == shownView)
            return;
        ShowOn(*view);
        shownView = view;
        shownRevision = motor.History().Revision();
        stale = false;
        RefreshToolbar();
    }

    void ScopeArea::ShowOn(TraceView& view)
    {
        if (mode == ScopeMode::capture && motor.Captures().Last())
        {
            captureTrace.emplace(*motor.Captures().Last(), ControlRate());
            const auto count = captureTrace->Count();
            view.Show(&*captureTrace, { captureTrace->Time(0), captureTrace->Time(count - 1) }, 0.0);
            return;
        }

        const TraceSource* source = nullptr;
        if (mode == ScopeMode::frozen)
            source = &*frozenTrace;
        else
            source = &liveTrace.emplace(motor.History(), ControlRate());
        if (source->Count() == 0)
        {
            view.Show(nullptr, {}, 0.0);
            return;
        }
        const auto end = source->Time(source->Count() - 1);
        view.Show(source, { end - SpanSeconds(), end }, end);
    }

    void ScopeArea::RefreshToolbar()
    {
        freeze->setText(mode == ScopeMode::frozen ? "Run" : "Freeze");
        freeze->setToolTip(mode == ScopeMode::frozen ? "Follow the newest window again" : "Keep the traces on screen while the motor runs on");
        freeze->setEnabled(mode != ScopeMode::capture);
        span->setEnabled(mode != ScopeMode::capture);
        live->setVisible(mode == ScopeMode::capture);
        save->setEnabled(mode == ScopeMode::capture && motor.Captures().Last().has_value());

        QString text;
        if (mode == ScopeMode::capture && motor.Captures().Last())
            text = QString("Capture %1: %2 periods, time from the trigger").arg(motor.Captures().Last()->number).arg(motor.Captures().Last()->periods.size());
        else
            text = mode == ScopeMode::frozen ? "Frozen" : "Live";
        if (mode != ScopeMode::capture && motor.History().Dropped() != 0)
            text += QString(" · %1 windows dropped").arg(motor.History().Dropped());
        modeLabel->setText(text);
        ui::backend::qt::StyleStatusLabel(*modeLabel, mode == ScopeMode::live ? ui::theme::StatusLevel::Ok : ui::theme::StatusLevel::Warning);
        const auto* view = CurrentView();
        cursors->setText(view != nullptr ? QString::fromStdString(view->CursorSummary()) : QString{});
    }

    uint32_t ScopeArea::ControlRate() const
    {
        return motor.Identity() ? motor.Identity()->controlRateHz : defaultControlRateHz;
    }

    double ScopeArea::SpanSeconds() const
    {
        return spans[static_cast<std::size_t>(span->currentIndex())].seconds;
    }
}
