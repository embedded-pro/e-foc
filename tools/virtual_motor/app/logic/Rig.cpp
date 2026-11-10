#include "tools/virtual_motor/app/logic/Rig.hpp"
#include "tools/virtual_motor/app/logic/MotorText.hpp"
#include <algorithm>
#include <bit>
#include <filesystem>

namespace virtual_motor::app
{
    RigMotor::RigMotor(const MotorSpec& spec, Rig& rig)
        : spec(spec)
        , rig(rig)
    {}

    const MotorSpec& RigMotor::Spec() const
    {
        return spec;
    }

    MotorState RigMotor::State() const
    {
        return state;
    }

    const std::optional<port::Hello>& RigMotor::Identity() const
    {
        return identity;
    }

    const std::optional<port::Status>& RigMotor::Status() const
    {
        return status;
    }

    const std::optional<port::Changes>& RigMotor::Changes() const
    {
        return changes;
    }

    const std::deque<std::string>& RigMotor::Log() const
    {
        return log;
    }

    uint8_t RigMotor::ActiveDisturbances() const
    {
        return changes ? static_cast<uint8_t>(std::popcount(changes->differs)) : uint8_t{ 0 };
    }

    void RigMotor::Attach(std::unique_ptr<MotorLink> newLink)
    {
        link = std::move(newLink);
        identity = link->Identity();
        status = link->LastStatus();
        changes = link->LastChanges();
        link->Observe(*this);
        SetState(status && status->runState == port::RunState::paused ? MotorState::paused : MotorState::running);
    }

    void RigMotor::Poll()
    {
        if (link)
            link->Poll();
        if (link && !link->IsOpen())
            link.reset();
    }

    void RigMotor::Append(const std::string& text)
    {
        const auto line = PrintableLogLine(text);
        log.push_back(line);
        if (log.size() > logLines)
            log.pop_front();
        if (rig.observer != nullptr)
            rig.observer->MotorLogged(spec.node, line);
    }

    void RigMotor::SetState(MotorState newState)
    {
        state = newState;
        if (rig.observer != nullptr)
            rig.observer->MotorUpdated(spec.node);
    }

    void RigMotor::StatusChanged(const port::Status& update)
    {
        status = update;
        SetState(update.runState == port::RunState::paused ? MotorState::paused : MotorState::running);
    }

    void RigMotor::ChangesChanged(const port::Changes& update)
    {
        changes = update;
        if (rig.observer != nullptr)
            rig.observer->MotorUpdated(spec.node);
    }

    void RigMotor::RequestRefused(const port::Refused& refused)
    {
        if (rig.observer != nullptr)
            rig.observer->MotorRefused(spec.node, refused);
    }

    void RigMotor::LogLine(const std::string& line)
    {
        Append(line);
    }

    void RigMotor::LinkClosed()
    {
        if (process.has_value() && !stopping)
            SetState(MotorState::restarting);
    }

    Rig::Rig(EngineProcesses& processes, RigStore& store, std::string dataFolder, uint16_t hubPort)
        : processes(processes)
        , store(store)
        , dataFolder(std::move(dataFolder))
        , hubPort(hubPort)
    {
        listener.onIntroduced = [this](std::unique_ptr<MotorLink> link)
        {
            Introduced(std::move(link));
        };
    }

    void Rig::Observe(RigObserver& rigObserver)
    {
        observer = &rigObserver;
    }

    bool Rig::Open()
    {
        if (!listener.Listen())
            return false;

        for (const auto& spec : store.Load())
            if (!Find(spec.node))
            {
                motors.push_back(std::make_unique<RigMotor>(spec, *this));
                Start(*motors.back(), false);
            }

        if (observer != nullptr)
            observer->MotorsChanged();
        return true;
    }

    std::optional<uint16_t> Rig::PlantPort() const
    {
        return listener.Port();
    }

    void Rig::Poll()
    {
        listener.Poll();
        for (auto& motor : motors)
            motor->Poll();
    }

    std::optional<std::string> Rig::Add(const MotorSpec& spec)
    {
        if (Find(spec.node) != nullptr)
            return "Node " + std::to_string(spec.node) + " is already in the list; pick another node or remove that motor first.";

        motors.push_back(std::make_unique<RigMotor>(spec, *this));
        auto& motor = *motors.back();
        motor.spec.start = StartMode::keepMemory;
        Save();
        if (observer != nullptr)
            observer->MotorsChanged();
        Start(motor, spec.start == StartMode::factoryFresh);
        return std::nullopt;
    }

    void Rig::Remove(uint8_t node)
    {
        PowerOff(node);
        std::erase_if(motors, [node](const auto& motor)
            {
                return motor->spec.node == node;
            });
        Save();
        if (observer != nullptr)
            observer->MotorsChanged();
    }

    void Rig::PowerOn(uint8_t node)
    {
        if (auto* motor = Find(node); motor != nullptr && !motor->process.has_value())
            Start(*motor, false);
    }

    void Rig::PowerOff(uint8_t node)
    {
        auto* motor = Find(node);
        if (motor == nullptr || !motor->process.has_value())
            return;

        motor->stopping = true;
        processes.Stop(*motor->process);
    }

    void Rig::PowerCycle(uint8_t node)
    {
        auto* motor = Find(node);
        if (motor == nullptr)
            return;

        if (motor->process.has_value())
        {
            motor->stopping = true;
            processes.Stop(*motor->process);
        }
        Start(*motor, false);
    }

    void Rig::PowerOffAll()
    {
        for (auto& motor : motors)
            PowerOff(motor->spec.node);
    }

    const std::vector<std::unique_ptr<RigMotor>>& Rig::Motors() const
    {
        return motors;
    }

    RigMotor* Rig::Find(uint8_t node)
    {
        const auto found = std::ranges::find_if(motors, [node](const auto& motor)
            {
                return motor->spec.node == node;
            });
        return found != motors.end() ? found->get() : nullptr;
    }

    void Rig::ProcessOutput(ProcessId process, const std::string& line)
    {
        if (auto* motor = FindProcess(process))
            motor->Append(line);
    }

    void Rig::ProcessEnded(ProcessId process)
    {
        auto* motor = FindProcess(process);
        if (motor == nullptr)
            return;

        motor->process.reset();
        motor->link.reset();
        motor->status.reset();
        motor->SetState(motor->stopping ? MotorState::stopped : MotorState::crashed);
        motor->stopping = false;
    }

    void Rig::Start(RigMotor& motor, bool factoryFresh)
    {
        const auto folder = std::filesystem::path{ dataFolder } / MotorFolderName(motor.spec);
        std::error_code error;
        std::filesystem::create_directories(folder, error);

        motor.stopping = false;
        motor.status.reset();
        motor.changes.reset();
        motor.process = processes.Start(EngineArguments(motor.spec, { listener.Port().value_or(0), hubPort }, factoryFresh), folder.string());
        if (motor.process.has_value())
            motor.SetState(MotorState::starting);
        else
        {
            motor.Append("The virtual motor program could not be started.");
            motor.SetState(MotorState::crashed);
        }
    }

    void Rig::Introduced(std::unique_ptr<MotorLink> link)
    {
        const auto& hello = *link->Identity();
        auto* motor = Find(hello.node);
        if (motor == nullptr || hello.bus != motor->spec.bus || !motor->process.has_value())
            return;

        motor->Attach(std::move(link));
    }

    RigMotor* Rig::FindProcess(ProcessId process)
    {
        const auto found = std::ranges::find_if(motors, [process](const auto& motor)
            {
                return motor->process == process;
            });
        return found != motors.end() ? found->get() : nullptr;
    }

    void Rig::Save()
    {
        std::vector<MotorSpec> specs;
        for (const auto& motor : motors)
            specs.push_back(motor->spec);
        store.Save(specs);
    }
}
