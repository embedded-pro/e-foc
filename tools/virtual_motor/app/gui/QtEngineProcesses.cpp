#include "tools/virtual_motor/app/gui/QtEngineProcesses.hpp"
#include <QStringList>

namespace virtual_motor::app
{
    QtEngineProcesses::QtEngineProcesses(QString program, QObject* parent)
        : QObject(parent)
        , program(std::move(program))
    {}

    QtEngineProcesses::~QtEngineProcesses()
    {
        for (auto& [id, process] : running)
        {
            process->disconnect();
            process->kill();
            process->waitForFinished(1000);
        }
    }

    std::optional<ProcessId> QtEngineProcesses::Start(const std::vector<std::string>& arguments, const std::string& workingDirectory)
    {
        QStringList list;
        for (const auto& argument : arguments)
            list << QString::fromStdString(argument);

        auto* process = new QProcess(this);
        process->setWorkingDirectory(QString::fromStdString(workingDirectory));
        process->setStandardOutputFile(QProcess::nullDevice());
        process->start(program, list);
        if (!process->waitForStarted(5000))
        {
            process->deleteLater();
            return std::nullopt;
        }

        const auto id = ++next;
        running[id] = process;
        connect(process, &QProcess::readyReadStandardError, this, [this, id, process]()
            {
                ReadErrors(id, *process);
            });
        connect(process, &QProcess::finished, this, [this, id, process]()
            {
                ReadErrors(id, *process);
                running.erase(id);
                process->deleteLater();
                if (onEnded)
                    onEnded(id);
            });
        return id;
    }

    void QtEngineProcesses::Stop(ProcessId id)
    {
        if (const auto found = running.find(id); found != running.end())
            found->second->kill();
    }

    void QtEngineProcesses::ReadErrors(ProcessId id, QProcess& process)
    {
        while (process.canReadLine())
        {
            const auto line = QString::fromUtf8(process.readLine()).trimmed();
            if (!line.isEmpty() && onOutput)
                onOutput(id, line.toStdString());
        }
    }
}
