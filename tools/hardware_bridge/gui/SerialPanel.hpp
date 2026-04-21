#pragma once

#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>

namespace tool
{
    class SerialPanel
        : public QGroupBox
    {
        Q_OBJECT

    public:
        explicit SerialPanel(QWidget* parent = nullptr);

        void AppendText(const QString& text);
        void SetConnected(bool connected);

    signals:
        void SendRequested(QString text);

    private slots:
        void OnSendClicked();

    private:
        QPlainTextEdit* outputView;
        QLineEdit* sendInput;
        QPushButton* sendButton;
    };
}
