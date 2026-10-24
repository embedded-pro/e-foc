#include "tools/can_commander/gui/EventLogView.hpp"
#include "ui/backend/qt/QtConversions.hpp"
#include "ui/theme/Theme.hpp"
#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QVBoxLayout>

namespace tool
{
    namespace
    {
        constexpr int maximumLines{ 5000 };
    }

    EventLogView::EventLogView(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        auto* toolbar = new QHBoxLayout();
        toolbar->addWidget(new QLabel("Event Log"));
        toolbar->addStretch();
        showFrames = new QCheckBox("Show CAN Fr&ames");
        showFrames->setToolTip("Also list every CAN frame that is not decoded into telemetry or acknowledgements");
        toolbar->addWidget(showFrames);
        auto* saveButton = new QPushButton("Save…");
        saveButton->setToolTip("Save the event log to a text file");
        toolbar->addWidget(saveButton);
        auto* clearButton = new QPushButton("Clear");
        toolbar->addWidget(clearButton);
        layout->addLayout(toolbar);

        text = new QPlainTextEdit();
        text->setReadOnly(true);
        text->setMaximumBlockCount(maximumLines);
        text->setFont(ui::backend::qt::ToQt(ui::theme::Current().Get(ui::theme::FontRole::Monospace)));
        text->setAccessibleName("Event log");
        layout->addWidget(text, 1);

        auto* scrollBar = text->verticalScrollBar();
        connect(scrollBar, &QScrollBar::rangeChanged, this, [this, scrollBar](int, int maximum)
            {
                if (followingTail)
                    scrollBar->setValue(maximum);
            });
        connect(scrollBar, &QScrollBar::valueChanged, this, [this, scrollBar](int value)
            {
                followingTail = value >= scrollBar->maximum();
            });

        connect(showFrames, &QCheckBox::toggled, this, &EventLogView::ShowFramesChanged);
        connect(saveButton, &QPushButton::clicked, this, &EventLogView::SaveAs);
        connect(clearButton, &QPushButton::clicked, this, &EventLogView::Clear);
    }

    void EventLogView::Append(IssueSeverity severity, const QString& message)
    {
        AppendLine(QString("%1  %2").arg(ToString(severity), message));
    }

    void EventLogView::AppendFrame(bool transmitted, uint32_t id, const CanFrame& frame)
    {
        if (!showFrames->isChecked())
            return;

        QString hex;
        for (std::size_t i = 0; i < frame.size(); ++i)
            hex += QString(i == 0 ? "%1" : " %1").arg(frame[i], 2, 16, QChar('0')).toUpper();

        AppendLine(QString("%1  ID 0x%2  [%3]").arg(transmitted ? "TX   " : "RX   ").arg(id, 8, 16, QChar('0')).arg(hex));
    }

    bool EventLogView::ShowsFrames() const
    {
        return showFrames->isChecked();
    }

    void EventLogView::SetShowsFrames(bool show)
    {
        showFrames->setChecked(show);
    }

    void EventLogView::Clear()
    {
        text->clear();
    }

    void EventLogView::SaveAs()
    {
        const auto suggested = QString("can_commander_%1.log").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        const auto path = QFileDialog::getSaveFileName(this, "Save Event Log", suggested, "Log files (*.log *.txt);;All files (*)");
        if (path.isEmpty())
            return;

        QSaveFile file{ path };
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            file.write(text->toPlainText().toUtf8());
            if (file.commit())
                return;
        }

        QMessageBox::warning(this, "Save Event Log", QString("Could not save the log to %1:\n%2").arg(path, file.errorString()));
    }

    void EventLogView::AppendLine(const QString& line)
    {
        text->appendPlainText(QString("%1  %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), line));
    }
}
