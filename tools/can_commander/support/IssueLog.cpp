#include "tools/can_commander/support/IssueLog.hpp"
#include <QDateTime>
#include <QDir>
#include <algorithm>
#include <atomic>

namespace tool
{
    namespace
    {
        // Qt's message handler is process-wide; this is the one log it forwards to.
        std::atomic<IssueLog*> capturedLog{ nullptr };
        QtMessageHandler previousHandler{ nullptr };

        // A warning raised while a write is in progress (QFile on a vanished folder) must not
        // re-enter the write and take its lock a second time.
        thread_local bool writing{ false };

        void ForwardQtMessage(QtMsgType type, const QMessageLogContext& context, const QString& message)
        {
            auto* log = capturedLog.load();
            if (log != nullptr && !writing && type != QtDebugMsg && type != QtInfoMsg)
                log->Write(type == QtWarningMsg ? IssueSeverity::warning : IssueSeverity::error, "Qt: " + message);

            if (previousHandler != nullptr)
                previousHandler(type, context, message);
        }
    }

    QString ToString(IssueSeverity severity)
    {
        switch (severity)
        {
            case IssueSeverity::warning:
                return "WARN ";
            case IssueSeverity::error:
                return "ERROR";
            default:
                return "INFO ";
        }
    }

    IssueLog::IssueLog(const Config& config)
        : config(config)
    {
        this->config.keptFiles = std::max(this->config.keptFiles, 1);
        QDir{}.mkpath(config.directory);
        Open();
    }

    IssueLog::~IssueLog()
    {
        if (capturing)
        {
            capturedLog = nullptr;
            qInstallMessageHandler(previousHandler);
            previousHandler = nullptr;
        }
    }

    void IssueLog::Write(IssueSeverity severity, const QString& message)
    {
        auto line = QString("%1 [%2] %3")
                        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs), ToString(severity), message.simplified());

        std::scoped_lock lock{ mutex };
        writing = true;
        Remember(line);

        if (file.isOpen())
            RotateIfNeeded();

        if (file.isOpen())
        {
            file.write(line.append('\n').toUtf8());
            file.flush();
        }

        writing = false;
    }

    bool IssueLog::IsWritable() const
    {
        return file.isOpen();
    }

    QString IssueLog::FilePath() const
    {
        return RotatedPath(0);
    }

    QString IssueLog::Directory() const
    {
        return config.directory;
    }

    QStringList IssueLog::RecentLines(std::size_t count) const
    {
        std::scoped_lock lock{ mutex };
        QStringList lines;
        const auto skip = recent.size() > count ? recent.size() - count : 0;
        for (auto it = recent.begin() + static_cast<std::ptrdiff_t>(skip); it != recent.end(); ++it)
            lines.append(*it);
        return lines;
    }

    void IssueLog::CaptureQtMessages()
    {
        if (capturing || capturedLog.load() != nullptr)
            return;

        capturing = true;
        previousHandler = qInstallMessageHandler(ForwardQtMessage);
        capturedLog = this;
    }

    QString IssueLog::RotatedPath(int index) const
    {
        const auto name = index == 0 ? QString("%1.log").arg(config.baseName) : QString("%1.%2.log").arg(config.baseName).arg(index);
        return QDir{ config.directory }.filePath(name);
    }

    void IssueLog::Open()
    {
        file.setFileName(FilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            return;

        RotateIfNeeded();
    }

    void IssueLog::RotateIfNeeded()
    {
        if (file.size() < config.maxFileBytes)
            return;

        file.close();
        QFile::remove(RotatedPath(config.keptFiles - 1));
        for (int index = config.keptFiles - 2; index >= 0; --index)
            QFile::rename(RotatedPath(index), RotatedPath(index + 1));

        file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }

    void IssueLog::Remember(const QString& line)
    {
        recent.push_back(line);
        while (recent.size() > config.recentCapacity)
            recent.pop_front();
    }
}
