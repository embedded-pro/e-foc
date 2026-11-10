#pragma once

#include "tools/virtual_motor/app/logic/EngineProcesses.hpp"
#include "tools/virtual_motor/app/logic/MotorLink.hpp"
#include "tools/virtual_motor/app/logic/MotorSpec.hpp"
#include "tools/virtual_motor/app/logic/PortListener.hpp"
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace virtual_motor::app
{
    class RigObserver
    {
    public:
        virtual void MotorsChanged() = 0;
        virtual void MotorUpdated(uint8_t node) = 0;
        virtual void MotorLogged(uint8_t node, const std::string& line) = 0;
        virtual void MotorRefused(uint8_t node, const port::Refused& refused) = 0;

    protected:
        ~RigObserver() = default;
    };

    class RigMotor
        : private MotorLinkObserver
    {
    public:
        static constexpr std::size_t logLines{ 5000 };

        RigMotor(const MotorSpec& spec, class Rig& rig);

        const MotorSpec& Spec() const;
        MotorState State() const;
        const std::optional<port::Hello>& Identity() const;
        const std::optional<port::Status>& Status() const;
        const std::optional<port::Changes>& Changes() const;
        const std::deque<std::string>& Log() const;
        uint8_t ActiveDisturbances() const;

        template<class Request>
        void Send(const Request& request)
        {
            if (link)
                link->Send(request);
        }

    private:
        friend class Rig;

        void Attach(std::unique_ptr<MotorLink> newLink);
        void Poll();
        void Append(const std::string& line);
        void SetState(MotorState newState);

        void StatusChanged(const port::Status& status) override;
        void ChangesChanged(const port::Changes& changes) override;
        void RequestRefused(const port::Refused& refused) override;
        void LogLine(const std::string& line) override;
        void LinkClosed() override;

        MotorSpec spec;
        Rig& rig;
        MotorState state{ MotorState::stopped };
        std::optional<ProcessId> process;
        bool stopping{ false };
        std::unique_ptr<MotorLink> link;
        std::optional<port::Hello> identity;
        std::optional<port::Status> status;
        std::optional<port::Changes> changes;
        std::deque<std::string> log;
    };

    class Rig
    {
    public:
        Rig(EngineProcesses& processes, RigStore& store, std::string dataFolder, uint16_t hubPort);

        void Observe(RigObserver& observer);
        bool Open();
        std::optional<uint16_t> PlantPort() const;
        void Poll();

        std::optional<std::string> Add(const MotorSpec& spec);
        void Remove(uint8_t node);
        void PowerOn(uint8_t node);
        void PowerOff(uint8_t node);
        void PowerCycle(uint8_t node);
        void PowerOffAll();

        const std::vector<std::unique_ptr<RigMotor>>& Motors() const;
        RigMotor* Find(uint8_t node);

        void ProcessOutput(ProcessId process, const std::string& line);
        void ProcessEnded(ProcessId process);

    private:
        friend class RigMotor;

        void Start(RigMotor& motor, bool factoryFresh);
        void Introduced(std::unique_ptr<MotorLink> link);
        RigMotor* FindProcess(ProcessId process);
        void Save();

        EngineProcesses& processes;
        RigStore& store;
        std::string dataFolder;
        uint16_t hubPort;
        PortListener listener;
        RigObserver* observer{ nullptr };
        std::vector<std::unique_ptr<RigMotor>> motors;
    };
}
