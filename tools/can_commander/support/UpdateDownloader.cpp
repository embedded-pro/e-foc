#include "tools/can_commander/support/UpdateDownloader.hpp"
#include "tools/can_commander/support/ApplicationInfo.hpp"
#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace tool
{
    namespace
    {
        constexpr int inactivityTimeoutMs{ 30000 };
    }

    UpdateDownloader::UpdateDownloader(QObject* parent)
        : QObject(parent)
    {}

    void UpdateDownloader::Start(const InstallerAsset& installer, const QString& directory)
    {
        if (IsDownloading())
            return;

        asset = installer;
        cancelled = false;
        hash.reset();

        if (!asset.sha256)
        {
            FailLater(QString("the release publishes no checksum for %1, so it cannot be installed automatically").arg(asset.fileName));
            return;
        }

        if (asset.fileName.isEmpty() || asset.fileName.contains('/') || asset.fileName.contains('\\'))
        {
            FailLater(QString("the release names its installer \"%1\", which is not a plain file name").arg(asset.fileName));
            return;
        }

        if (!QDir{}.mkpath(directory))
        {
            FailLater(QString("cannot create the download folder %1").arg(QDir::toNativeSeparators(directory)));
            return;
        }

        targetPath = QDir{ directory }.filePath(asset.fileName);
        file = std::make_unique<QFile>(targetPath + ".part");
        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            FailLater(QString("cannot write %1: %2").arg(QDir::toNativeSeparators(file->fileName()), file->errorString()));
            file.reset();
            return;
        }

        QNetworkRequest request{ asset.url };
        request.setHeader(QNetworkRequest::UserAgentHeader, QByteArray{ "e-foc-can-commander/" }.append(application::version.data(), static_cast<qsizetype>(application::version.size())));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(inactivityTimeoutMs);

        reply = network.get(request);
        connect(reply, &QNetworkReply::readyRead, this, &UpdateDownloader::OnReadyRead);
        connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total)
            {
                emit Progress(received, total > 0 ? total : asset.size);
            });
        connect(reply, &QNetworkReply::finished, this, &UpdateDownloader::OnFinished);
    }

    void UpdateDownloader::Cancel()
    {
        if (!IsDownloading())
            return;

        cancelled = true;
        reply->abort();
    }

    bool UpdateDownloader::IsDownloading() const
    {
        return !reply.isNull();
    }

    void UpdateDownloader::FailLater(const QString& reason)
    {
        QTimer::singleShot(0, this, [this, reason]()
            {
                emit Failed(reason);
            });
    }

    void UpdateDownloader::OnReadyRead()
    {
        if (!Append(reply->readAll()))
            reply->abort();
    }

    bool UpdateDownloader::Append(const QByteArray& data)
    {
        hash.addData(data);
        return file->write(data) == data.size();
    }

    void UpdateDownloader::OnFinished()
    {
        auto* finished = reply.data();
        finished->deleteLater();
        reply.clear();

        if (cancelled)
        {
            Discard();
            return;
        }

        if (finished->error() == QNetworkReply::NoError)
            Append(finished->readAll());

        if (file->error() != QFileDevice::NoError)
        {
            const auto reason = QString("cannot write %1: %2").arg(QDir::toNativeSeparators(file->fileName()), file->errorString());
            Discard();
            emit Failed(reason);
            return;
        }

        if (finished->error() != QNetworkReply::NoError)
        {
            const auto reason = finished->errorString();
            Discard();
            emit Failed(reason);
            return;
        }

        file->close();
        if (const auto mismatch = Verify(); !mismatch.isEmpty())
        {
            Discard();
            emit Failed(mismatch);
            return;
        }

        QFile::remove(targetPath);
        if (!file->rename(targetPath))
        {
            const auto reason = QString("cannot move the download to %1: %2").arg(QDir::toNativeSeparators(targetPath), file->errorString());
            Discard();
            emit Failed(reason);
            return;
        }

        file.reset();
        emit Downloaded(targetPath);
    }

    QString UpdateDownloader::Verify() const
    {
        if (asset.size > 0 && file->size() != asset.size)
            return QString("the download is %1 bytes, but the release lists %2").arg(file->size()).arg(asset.size);

        if (hash.result() != *asset.sha256)
            return "the download does not match the checksum published with the release";

        return {};
    }

    void UpdateDownloader::Discard()
    {
        if (!file)
            return;

        file->close();
        file->remove();
        file.reset();
    }
}
