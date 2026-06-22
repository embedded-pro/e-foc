#include "core/state_machine/BoardProtectionFaultNotifier.hpp"

namespace state_machine
{
    BoardProtectionFaultNotifier::BoardProtectionFaultNotifier(application::PlatformFactory& platform)
    {
        platform.RegisterBoardProtection([this](application::PlatformFactory::BoardProtectionReason reason)
            {
                OnBoardProtection(reason);
            });
    }

    void BoardProtectionFaultNotifier::Register(const infra::Function<void(FaultCode)>& onFault)
    {
        stateMachineSink = onFault;
    }

    void BoardProtectionFaultNotifier::OnFaultBroadcast(const infra::Function<void(FaultCode)>& onBroadcast)
    {
        broadcastSink = onBroadcast;
    }

    void BoardProtectionFaultNotifier::OnBoardProtection(application::PlatformFactory::BoardProtectionReason reason)
    {
        const FaultCode code = ToFaultCode(reason);

        if (stateMachineSink)
            stateMachineSink(code);

        if (broadcastSink)
            broadcastSink(code);
    }

    FaultCode BoardProtectionFaultNotifier::ToFaultCode(application::PlatformFactory::BoardProtectionReason reason)
    {
        switch (reason)
        {
            case application::PlatformFactory::BoardProtectionReason::overCurrent:
                return FaultCode::overcurrent;
            case application::PlatformFactory::BoardProtectionReason::overVoltage:
                return FaultCode::overvoltage;
            case application::PlatformFactory::BoardProtectionReason::overTemperature:
                return FaultCode::overtemperature;
            default:
                return FaultCode::hardwareFault;
        }
    }
}
