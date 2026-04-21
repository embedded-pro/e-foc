#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <cstdint>

namespace tool
{
    class CanPanel
        : public QGroupBox
    {
        Q_OBJECT

    public:
        explicit CanPanel(QWidget* parent = nullptr);

        void AppendFrame(bool rx, quint32 id, bool extended, const QByteArray& data);
        void SetConnected(bool connected);

    signals:
        void SendRequested(quint32 id, bool extended, QByteArray data);

    private slots:
        void OnSendClicked();

    private:
        static constexpr int maxRows{ 1000 };

        QTableWidget* frameTable;
        QLineEdit* idInput;
        QLineEdit* dataInput;
        QPushButton* sendButton;
        QElapsedTimer elapsed;
    };
}
