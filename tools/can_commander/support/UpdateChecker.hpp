#pragma once

#include "tools/can_commander/support/ReleaseFeed.hpp"
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>

class QNetworkReply;

namespace tool
{
    class UpdateChecker
        : public QObject
    {
        Q_OBJECT

    public:
        UpdateChecker(const ReleaseVersion& current, const QUrl& feedUrl, QObject* parent = nullptr);

        void Check();
        bool IsChecking() const;

    signals:
        void UpdateAvailable(const tool::ReleaseInfo& release);
        void UpToDate(const tool::ReleaseInfo& latest);
        void CheckFailed(const QString& reason);

    private:
        void OnFinished(QNetworkReply* reply);

        ReleaseVersion current;
        QUrl feedUrl;
        QNetworkAccessManager network;
        QPointer<QNetworkReply> pending;
    };
}
