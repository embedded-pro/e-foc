#include "tools/virtual_motor/app/gui/LogView.hpp"
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

namespace virtual_motor::app
{
    LogView::LogView(QWidget* parent)
        : QWidget(parent)
        , filter(new QLineEdit(this))
        , text(new QPlainTextEdit(this))
    {
        filter->setPlaceholderText("Filter");
        filter->setAccessibleName("Log filter");
        text->setReadOnly(true);
        text->setMaximumBlockCount(maximumLines);
        text->setAccessibleName("Debug log");

        auto* clear = new QPushButton("Clear", this);
        auto* save = new QPushButton("Save…", this);
        auto* bar = new QHBoxLayout;
        bar->addWidget(filter, 1);
        bar->addWidget(clear);
        bar->addWidget(save);

        auto* layout = new QVBoxLayout(this);
        layout->addLayout(bar);
        layout->addWidget(text, 1);

        connect(filter, &QLineEdit::textChanged, this, &LogView::Refilter);
        connect(clear, &QPushButton::clicked, this, [this]()
            {
                lines.clear();
                text->clear();
            });
        connect(save, &QPushButton::clicked, this, &LogView::Save);
    }

    void LogView::Append(const QString& line)
    {
        lines << line;
        if (lines.size() > maximumLines)
            lines.removeFirst();
        if (filter->text().isEmpty() || line.contains(filter->text(), Qt::CaseInsensitive))
            text->appendPlainText(line);
    }

    void LogView::SetLines(const QStringList& newLines)
    {
        lines = newLines;
        Refilter();
    }

    void LogView::Refilter()
    {
        text->clear();
        const auto pattern = filter->text();
        for (const auto& line : lines)
            if (pattern.isEmpty() || line.contains(pattern, Qt::CaseInsensitive))
                text->appendPlainText(line);
    }

    void LogView::Save()
    {
        const auto path = QFileDialog::getSaveFileName(this, "Save Log", "virtual-motor.log", "Log files (*.log *.txt)");
        if (path.isEmpty())
            return;

        QFile file{ path };
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        QTextStream stream{ &file };
        for (const auto& line : lines)
            stream << line << '\n';
    }
}
