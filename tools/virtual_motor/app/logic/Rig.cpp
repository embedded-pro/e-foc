#include "tools/virtual_motor/app/logic/Rig.hpp"
#include "tools/virtual_motor/app/logic/MotorText.hpp"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <format>

namespace virtual_motor::app
{
    namespace
    {
        std::string RefusalText(port::RefusalReason reason)
        {
            switch (reason)
            {
                case port::RefusalReason::stepWhileRunning:
                    return "stepping needs a paused motor";
                case port::RefusalReason::unknownForm:
                    return "the motor does not know that form";
                default:
                    return "a value is out of range";
            }
        }
    }

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

    const TraceHistory& RigMotor::History() const
    {
        return history;
    }

    const CaptureAssembly& RigMotor::Captures() const
    {
        return captures;
    }

    const std::optional<port::ProfileState>& RigMotor::Profile() const
    {
        return profile;
    }

    bool RigMotor::Streaming() const
    {
        return streaming;
    }

    void RigMotor::Stream(bool on)
    {
        if (on == streaming)
            return;
        streaming = on;
        Send(port::StreamRequest{ static_cast<uint8_t>(on ? 1 : 0) });
    }

    void RigMotor::Attach(std::unique_ptr<MotorLink> newLink)
    {
        link = std::move(newLink);
        history.Clear();
        profile.reset();
        if (streaming)
            link->Send(port::StreamRequest{ 1 });
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
        rig.Report(IssueLevel::warning, std::format("Node {} refused a change: {}", static_cast<uint32_t>(spec.node), RefusalText(refused.reason)));
        if (rig.observer != nullptr)
            rig.observer->MotorRefused(spec.node, refused);
    }

    void RigMotor::LogLine(const std::string& line)
    {
        Append(line);
    }

    void RigMotor::WindowReceived(const port::Window& window)
    {
        history.Add(window);
    }

    void RigMotor::CaptureEventReceived(const port::CaptureEvent& event)
    {
        captures.Event(event);
        Captured();
    }

    void RigMotor::CaptureChunkReceived(const port::CaptureChunk& chunk)
    {
        captures.Chunk(chunk);
        Captured();
    }

    void RigMotor::ProfileStateReceived(const port::ProfileState& state)
    {
        profile = state;
        if (rig.observer != nullptr)
            rig.observer->MotorUpdated(spec.node);
    }

    void RigMotor::Captured()
    {
        if (rig.observer != nullptr)
            rig.observer->MotorCaptured(spec.node);
    }

    void RigMotor::LinkClosed()
    {
        if (captures.InFlight())
        {
            captures.Interrupted();
            Captured();
        }
        if (process.has_value() && !stopping)
        {
            rig.Report(IssueLevel::info, std::format("Node {} reset; waiting for it to reconnect", static_cast<uint32_t>(spec.node)));
            SetState(MotorState::restarting);
        }
    }

    Rig::Rig(EngineProcesses& processes, RigStore& store, std::string dataFolder, uint16_t hubPort)
        : processes(processes)
        , store(store)
        , dataFolder(std::move(dataFolder))
        , hubPort(hubPort)
    {
        listener.OnIntroduced([this](std::unique_ptr<MotorLink> link)
            {
                Introduced(std::move(link));
            });
    }

    void Rig::Observe(RigObserver& rigObserver)
    {
        observer = &rigObserver;
    }

    bool Rig::Open()
    {
        if (!listener.Listen())
            return false;

        Restore(store.Load());
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
        for (const auto& motor : motors)
            motor->Poll();
    }

    std::optional<std::string> Rig::Add(const MotorSpec& spec)
    {
        std::optional<std::string> refusal;
        if (Find(spec.node) != nullptr)
            refusal = std::format("Node {} is already in the list; pick another node or remove that motor first.", static_cast<uint32_t>(spec.node));
        else if (Full())
            refusal = "A virtual bus carries at most eight motors, the number the CAN Commander follows; remove one first.";
        if (refusal)
        {
            Report(IssueLevel::warning, *refusal);
            return refusal;
        }

        motors.push_back(std::make_unique<RigMotor>(spec, *this));
        auto& motor = *motors.back();
        Report(IssueLevel::info, std::format("Node {} added with the {} plant", static_cast<uint32_t>(spec.node), spec.plant));
        motor.spec.start = StartMode::keepMemory;
        Save();
        if (observer != nullptr)
            observer->MotorsChanged();
        Start(motor, spec.start == StartMode::factoryFresh);
        return std::nullopt;
    }

    bool Rig::Full() const
    {
        return motors.size() >= maxMotors;
    }

    void Rig::Remove(uint8_t node)
    {
        if (Find(node) != nullptr)
            Report(IssueLevel::info, std::format("Node {} removed", static_cast<uint32_t>(node)));
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
        for (const auto& motor : motors)
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
        if (!motor->stopping)
            Report(IssueLevel::error, std::format("Node {} crashed: the virtual motor program ended unexpectedly{}", static_cast<uint32_t>(motor->spec.node), motor->log.empty() ? std::string{} : "; its last output: " + motor->log.back()));
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
            Report(IssueLevel::error, std::format("Node {}: the virtual motor program could not be started", static_cast<uint32_t>(motor.spec.node)));
            motor.SetState(MotorState::crashed);
        }
    }

    void Rig::Introduced(std::unique_ptr<MotorLink> link)
    {
        const auto& hello = *link->Identity();
        auto* motor = Find(hello.node);
        if (motor == nullptr || hello.bus != motor->spec.bus || !motor->process.has_value())
            return;

        if (hello.version != port::protocolVersion)
        {
            const auto mismatch = std::format("The virtual motor speaks plant port protocol {} and this app {}: install the virtual motor program that came with this app", hello.version, port::protocolVersion);
            motor->Append(mismatch);
            Report(IssueLevel::error, std::format("Node {}: {}", static_cast<uint32_t>(hello.node), mismatch));
            PowerOff(hello.node);
            return;
        }

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

    void Rig::Report(IssueLevel level, const std::string& text) const
    {
        if (observer != nullptr)
            observer->Reported(level, text);
    }

    void Rig::Restore(const std::vector<MotorSpec>& specs)
    {
        std::string skipped;
        for (const auto& spec : specs)
        {
            if (Find(spec.node) != nullptr)
                continue;
            if (Full())
            {
                skipped += (skipped.empty() ? "" : ", ") + std::to_string(spec.node);
                continue;
            }
            motors.push_back(std::make_unique<RigMotor>(spec, *this));
            Start(*motors.back(), false);
        }

        if (!skipped.empty())
            Report(IssueLevel::warning, std::format("The saved list holds more than eight motors; nodes {} were not started", skipped));
    }

    void Rig::Save()
    {
        std::vector<MotorSpec> specs;
        for (const auto& motor : motors)
            specs.push_back(motor->spec);
        store.Save(specs);
    }
}
