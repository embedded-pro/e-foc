#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/state_machine/FaultNotifier.hpp"

namespace state_machine
{
    class PlatformFaultNotifier
        : public FaultNotifier
    {
    public:
        explicit PlatformFaultNotifier(application::PlatformFactory& platform);

        void Register(const infra::Function<void(FaultCode)>& onFault) override;
        void RegisterSecondary(const infra::Function<void(FaultCode)>& onFault);

    private:
        static FaultCode ToFaultCode(application::PlatformFactory::BoardProtectionReason reason);

        application::PlatformFactory& platform;
        infra::Function<void(FaultCode)> onFault;
        infra::Function<void(FaultCode)> onFaultSecondary;
    };
}
