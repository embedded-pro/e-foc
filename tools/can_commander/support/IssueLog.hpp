#pragma once

#include <QFile>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <cstddef>
#include <deque>
#include <mutex>

namespace tool
{
    enum class IssueSeverity
    {
        info,
        warning,
        error
    };

    class IssueLog
    {
    public:
        struct Config
        {
            QString directory;
            QString baseName{ "can_commander" };
            qint64 maxFileBytes{ 1024 * 1024 };
            int keptFiles{ 3 };
            std::size_t recentCapacity{ 200 };
        };

        explicit IssueLog(const Config& config);
        ~IssueLog();

        IssueLog(const IssueLog&) = delete;
        IssueLog& operator=(const IssueLog&) = delete;

        void Write(IssueSeverity severity, const QString& message);

        bool IsWritable() const;
        QString FilePath() const;
        QString Directory() const;
        QStringList RecentLines(std::size_t count) const;

        void CaptureQtMessages();

    private:
        QString RotatedPath(int index) const;
        void Open();
        bool OpenForAppend();
        void RotateIfNeeded();
        void Remember(const QString& line);

        Config config;
        QFile file;
        std::deque<QString> recent;
        mutable std::mutex mutex;
        bool capturing{ false };
    };

    QString ToString(IssueSeverity severity);
}
