#include "tools/hardware_bridge/client/gui/SerialPanel.hpp"
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>

namespace tool
{
    SerialPanel::SerialPanel(QWidget* parent)
        : QGroupBox("Serial", parent)
    {
        auto* layout = new QVBoxLayout(this);

        outputView = new QPlainTextEdit();
        outputView->setReadOnly(true);
        outputView->setFont(QFont("Monospace", 9));
        outputView->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        outputView->setMaximumBlockCount(5000);
        layout->addWidget(outputView, 1);

        auto* sendRow = new QHBoxLayout();
        sendRow->addWidget(new QLabel("ASCII:"));
        sendInput = new QLineEdit();
        sendInput->setPlaceholderText("text to send...");
        sendRow->addWidget(sendInput, 1);
        sendButton = new QPushButton("Send");
        sendButton->setEnabled(false);
        sendRow->addWidget(sendButton);
        layout->addLayout(sendRow);

        connect(sendButton, &QPushButton::clicked, this, &SerialPanel::OnSendClicked);
        connect(sendInput, &QLineEdit::returnPressed, this, &SerialPanel::OnSendClicked);
    }

    void SerialPanel::AppendText(const QString& text)
    {
        QTextCursor cursor = outputView->textCursor();
        cursor.movePosition(QTextCursor::End);
        outputView->setTextCursor(cursor);
        outputView->insertPlainText(text);
        outputView->verticalScrollBar()->setValue(outputView->verticalScrollBar()->maximum());
    }

    void SerialPanel::SetConnected(bool connected)
    {
        sendButton->setEnabled(connected);
    }

    void SerialPanel::OnSendClicked()
    {
        const QString text = sendInput->text();
        if (text.isEmpty())
            return;
        emit SendRequested(text);
        sendInput->clear();
    }
}
