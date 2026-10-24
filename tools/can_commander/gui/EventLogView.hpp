#pragma once

#include "tools/can_commander/adapter/CanBusAdapter.hpp"
#include "tools/can_commander/support/IssueLog.hpp"
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QWidget>

namespace tool
{
    class EventLogView
        : public QWidget
    {
        Q_OBJECT

    public:
        explicit EventLogView(QWidget* parent = nullptr);

        void Append(IssueSeverity severity, const QString& message);
        void AppendFrame(bool transmitted, uint32_t id, const CanFrame& data);

        bool ShowsFrames() const;

    signals:
        void ShowFramesChanged(bool show);

    public slots:
        void SetShowsFrames(bool show);
        void Clear();
        void SaveAs();

    private:
        void AppendLine(const QString& text);

        QPlainTextEdit* text;
        QCheckBox* showFrames;
    };
}
