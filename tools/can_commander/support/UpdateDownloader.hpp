#pragma once

#include "tools/can_commander/support/ReleaseFeed.hpp"
#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <memory>

class QNetworkReply;

namespace tool
{
    class UpdateDownloader
        : public QObject
    {
        Q_OBJECT

    public:
        explicit UpdateDownloader(QObject* parent = nullptr);

        void Start(const InstallerAsset& asset, const QString& directory);
        void Cancel();
        bool IsDownloading() const;

    signals:
        void Progress(qint64 received, qint64 total);
        void Downloaded(const QString& filePath);
        void Failed(const QString& reason);

    private:
        void FailLater(const QString& reason);
        void OnReadyRead();
        bool Append(const QByteArray& data);
        void OnFinished();
        QString Verify() const;
        void Discard();

        QNetworkAccessManager network;
        QPointer<QNetworkReply> reply;
        std::unique_ptr<QFile> file;
        QCryptographicHash hash{ QCryptographicHash::Sha256 };
        InstallerAsset asset;
        QString targetPath;
        bool cancelled{ false };
    };
}
