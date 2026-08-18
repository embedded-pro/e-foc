#include "core/state_machine/PlatformFaultNotifier.hpp"

namespace state_machine
{
    PlatformFaultNotifier::PlatformFaultNotifier(application::PlatformFactory& platform)
        : platform(platform)
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
        platform.CanBus().SetOnError([this](application::CanBusAdapter::CanError error)
            {
                if (error == application::CanBusAdapter::CanError::busOff && this->onFault != nullptr)
                    this->onFault(FaultCode::hardwareFault);
            });
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
