#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <cstdint>

namespace tool
{
    class CanBusAdapter : public QObject
    {
        Q_OBJECT

    public:
        using QObject::QObject;
        ~CanBusAdapter() override = default;

        CanBusAdapter(const CanBusAdapter&) = delete;
        CanBusAdapter& operator=(const CanBusAdapter&) = delete;

        virtual bool Connect(const QString& interface, uint32_t bitrate) = 0;
        virtual void Disconnect() = 0;
        virtual bool IsConnected() const = 0;
        virtual bool Send(uint32_t id, const QByteArray& data) = 0;

    signals:
        void FrameReceived(uint32_t id, QByteArray data);
        void ErrorOccurred(QString message);
        void ConnectionChanged(bool connected);
    };
}
