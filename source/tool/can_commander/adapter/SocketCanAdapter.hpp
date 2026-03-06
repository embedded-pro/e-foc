#pragma once

#include "source/tool/can_commander/adapter/CanBusAdapter.hpp"

#ifdef __linux__

#include <QSocketNotifier>

namespace tool
{
    class SocketCanAdapter : public CanBusAdapter
    {
        Q_OBJECT

    public:
        explicit SocketCanAdapter(QObject* parent = nullptr);
        ~SocketCanAdapter() override;

        bool Connect(const QString& interface, uint32_t bitrate) override;
        void Disconnect() override;
        bool IsConnected() const override;
        bool Send(uint32_t id, const QByteArray& data) override;

    private:
        void OnReadyRead();

        int socketDescriptor = -1;
        QSocketNotifier* readNotifier = nullptr;
    };
}

#endif
