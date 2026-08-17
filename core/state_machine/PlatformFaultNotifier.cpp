#include "core/state_machine/PlatformFaultNotifier.hpp"

namespace state_machine
{
    PlatformFaultNotifier::PlatformFaultNotifier(application::PlatformFactory& platform)
    {
        platform.RegisterBoardProtection([this](application::PlatformFactory::BoardProtectionReason reason)
            {
                if (onFault != nullptr)
                    onFault(ToFaultCode(reason));
            });
    }

    void PlatformFaultNotifier::Register(const infra::Function<void(FaultCode)>& onFault)
    {
        this->onFault = onFault;
    }

    FaultCode PlatformFaultNotifier::ToFaultCode(application::PlatformFactory::BoardProtectionReason reason)
    {
        switch (reason)
        {
            case application::PlatformFactory::BoardProtectionReason::overCurrent:
                return FaultCode::overcurrent;
            case application::PlatformFactory::BoardProtectionReason::overVoltage:
                return FaultCode::overvoltage;
            case application::PlatformFactory::BoardProtectionReason::overTemperature:
                return FaultCode::overtemperature;
        }

        return FaultCode::hardwareFault;
    }
}
