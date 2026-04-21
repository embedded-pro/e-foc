#include "tools/hardware_bridge/gui/CanPanel.hpp"
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace tool
{
    CanPanel::CanPanel(QWidget* parent)
        : QGroupBox("CAN", parent)
    {
        auto* layout = new QVBoxLayout(this);

        frameTable = new QTableWidget(0, 5);
        frameTable->setHorizontalHeaderLabels({ "Dir", "Time (s)", "ID (hex)", "DLC", "Data (hex)" });
        frameTable->horizontalHeader()->setStretchLastSection(true);
        frameTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        frameTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        frameTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        frameTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        frameTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        frameTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        frameTable->verticalHeader()->setVisible(false);
        frameTable->setFont(QFont("Monospace", 9));
        layout->addWidget(frameTable, 1);

        auto* sendGroup = new QGroupBox("Send");
        auto* sendLayout = new QHBoxLayout(sendGroup);
        sendLayout->addWidget(new QLabel("ID (hex):"));
        idInput = new QLineEdit();
        idInput->setPlaceholderText("0x123");
        idInput->setFixedWidth(80);
        sendLayout->addWidget(idInput);
        sendLayout->addWidget(new QLabel("Data (hex bytes):"));
        dataInput = new QLineEdit();
        dataInput->setPlaceholderText("DE AD BE EF");
        sendLayout->addWidget(dataInput, 1);
        sendButton = new QPushButton("Send Frame");
        sendButton->setEnabled(false);
        sendLayout->addWidget(sendButton);
        layout->addWidget(sendGroup);

        connect(sendButton, &QPushButton::clicked, this, &CanPanel::OnSendClicked);
        connect(dataInput, &QLineEdit::returnPressed, this, &CanPanel::OnSendClicked);
    }

    void CanPanel::AppendFrame(bool rx, quint32 id, bool extended, const QByteArray& data)
    {
        if (frameTable->rowCount() >= maxRows)
            frameTable->removeRow(0);

        const int row = frameTable->rowCount();
        frameTable->insertRow(row);

        auto* dirItem = new QTableWidgetItem(rx ? "RX" : "TX");
        dirItem->setForeground(rx ? Qt::darkGreen : Qt::darkBlue);
        frameTable->setItem(row, 0, dirItem);

        const double timeS = elapsed.isValid() ? static_cast<double>(elapsed.elapsed()) / 1000.0 : 0.0;
        frameTable->setItem(row, 1, new QTableWidgetItem(QString::number(timeS, 'f', 3)));

        const QString idStr = extended
                                  ? QString("0x%1").arg(id, 8, 16, QChar('0')).toUpper()
                                  : QString("0x%1").arg(id, 3, 16, QChar('0')).toUpper();
        frameTable->setItem(row, 2, new QTableWidgetItem(idStr));

        frameTable->setItem(row, 3, new QTableWidgetItem(QString::number(data.size())));

        QString dataStr;
        for (int i = 0; i < data.size(); ++i)
        {
            if (i > 0)
                dataStr += ' ';
            dataStr += QString("%1").arg(static_cast<uint8_t>(data[i]), 2, 16, QChar('0')).toUpper();
        }
        frameTable->setItem(row, 4, new QTableWidgetItem(dataStr));

        frameTable->scrollToBottom();
    }

    void CanPanel::SetConnected(bool connected)
    {
        sendButton->setEnabled(connected);
        if (connected)
            elapsed.start();
    }

    void CanPanel::OnSendClicked()
    {
        const QString idStr = idInput->text().trimmed();
        if (idStr.isEmpty())
        {
            QMessageBox::warning(this, "Send Error", "CAN ID must not be empty.");
            return;
        }

        bool idOk = false;
        const quint32 id = idStr.toUInt(&idOk, 0);
        if (!idOk)
        {
            QMessageBox::warning(this, "Send Error",
                QString("Invalid CAN ID: '%1'. Use decimal or 0x-prefixed hex.").arg(idStr));
            return;
        }

        const bool extended = id > 0x7FF;

        QByteArray payload;
        const QString dataStr = dataInput->text().trimmed();
        if (!dataStr.isEmpty())
        {
            const QStringList tokens = dataStr.split(' ', Qt::SkipEmptyParts);
            for (const QString& token : tokens)
            {
                bool byteOk = false;
                const uint byteVal = token.toUInt(&byteOk, 16);
                if (!byteOk || byteVal > 255)
                {
                    QMessageBox::warning(this, "Send Error",
                        QString("Invalid data byte: '%1'. Use space-separated hex bytes.").arg(token));
                    return;
                }
                payload.append(static_cast<char>(byteVal));
            }
        }

        if (payload.size() > 8)
        {
            QMessageBox::warning(this, "Send Error", "CAN data exceeds 8 bytes.");
            return;
        }

        emit SendRequested(id, extended, payload);
    }
}
