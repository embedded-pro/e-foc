#pragma once

#include "tools/virtual_motor/app/logic/MotorSpec.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace virtual_motor::app
{
    using ProcessId = uint32_t;

    class EngineProcesses
    {
    public:
        virtual std::optional<ProcessId> Start(const std::vector<std::string>& arguments, const std::string& workingDirectory) = 0;
        virtual void Stop(ProcessId process) = 0;

    protected:
        ~EngineProcesses() = default;
    };

    class RigStore
    {
    public:
        virtual std::vector<MotorSpec> Load() = 0;
        virtual void Save(const std::vector<MotorSpec>& motors) = 0;

    protected:
        ~RigStore() = default;
    };
}
