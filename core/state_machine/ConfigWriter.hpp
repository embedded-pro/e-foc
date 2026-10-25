#pragma once

#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "core/state_machine/NvmActivity.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "services/tracer/Tracer.hpp"
#include <chrono>

namespace state_machine
{
    class ConfigWriter
    {
    public:
        ConfigWriter(services::NonVolatileMemory& nvm, const services::ConfigData& config, services::Tracer& tracer,
            application::NvmActivity& nvmActivity);

        void Persist(const infra::Function<void(services::NvmStatus)>& onDone = nullptr);
        bool HasPendingPersist() const;

    private:
        void Write();
        void OnWritten(services::NvmStatus status);

        static constexpr std::chrono::milliseconds retryDelay{ 10 };

        services::NonVolatileMemory& nvm;
        const services::ConfigData& config;
        services::Tracer& tracer;
        application::NvmActivity& nvmActivity;
        infra::TimerSingleShot retryTimer;
        bool writing{ false };
        bool rewrite{ false };
        infra::Function<void(services::NvmStatus)> onWritten;
        infra::Function<void(services::NvmStatus)> onRewritten;
    };
}
