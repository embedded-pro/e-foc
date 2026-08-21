#include "core/state_machine/PlatformFaultNotifier.hpp"

namespace state_machine
{
    PlatformFaultNotifier::PlatformFaultNotifier(application::PlatformFactory& platform)
        : platform(platform)
    {
        platform.RegisterBoardProtection([this](application::PlatformFactory::BoardProtectionReason reason)
            {
                const auto code = ToFaultCode(reason);
                if (onFault != nullptr)
                    onFault(code);
                if (onFaultSecondary != nullptr)
                    onFaultSecondary(code);
            });
    }

    void PlatformFaultNotifier::Register(const infra::Function<void(FaultCode)>& onFault)
    {
        this->onFault = onFault;
        platform.CanBus().SetOnError([this](application::CanBusAdapter::CanError error)
            {
                if (error == application::CanBusAdapter::CanError::busOff)
                {
                    if (this->onFault != nullptr)
                        this->onFault(FaultCode::hardwareFault);
                    if (this->onFaultSecondary != nullptr)
                        this->onFaultSecondary(FaultCode::hardwareFault);
                }
            });
    }

    void PlatformFaultNotifier::RegisterSecondary(const infra::Function<void(FaultCode)>& onFault)
    {
        this->onFaultSecondary = onFault;
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
