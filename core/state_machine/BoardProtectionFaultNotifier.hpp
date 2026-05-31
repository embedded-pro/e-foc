#pragma once

#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/state_machine/FaultNotifier.hpp"
#include "infra/util/Function.hpp"

namespace state_machine
{
    class BoardProtectionFaultNotifier
        : public FaultNotifier
    {
    public:
        explicit BoardProtectionFaultNotifier(application::PlatformFactory& platform);

        void Register(const infra::Function<void(FaultCode)>& onFault) override;
        void OnFaultBroadcast(const infra::Function<void(FaultCode)>& onBroadcast);

    private:
        void OnBoardProtection(application::PlatformFactory::BoardProtectionReason reason);
        static FaultCode ToFaultCode(application::PlatformFactory::BoardProtectionReason reason);

        infra::Function<void(FaultCode)> stateMachineSink;
        infra::Function<void(FaultCode)> broadcastSink;
    };
}
