#pragma once

#include "tools/virtual_motor/app/logic/EngineProcesses.hpp"
#include <QObject>
#include <QProcess>
#include <QString>
#include <functional>
#include <map>

namespace virtual_motor::app
{
    class QtEngineProcesses
        : public QObject
        , public EngineProcesses
    {
        Q_OBJECT

    public:
        explicit QtEngineProcesses(QString program, QObject* parent = nullptr);
        ~QtEngineProcesses() override;

        std::optional<ProcessId> Start(const std::vector<std::string>& arguments, const std::string& workingDirectory) override;
        void Stop(ProcessId process) override;

        std::function<void(ProcessId)> onEnded;
        std::function<void(ProcessId, const std::string&)> onOutput;

    private:
        void ReadErrors(ProcessId id, QProcess& process);

        QString program;
        ProcessId next{ 0 };
        std::map<ProcessId, QProcess*> running;
    };
}
