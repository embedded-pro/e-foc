#include "core/state_machine/PlatformFaultNotifier.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace state_machine
{
    PlatformFaultNotifier::PlatformFaultNotifier(application::PlatformFactory& platform)
        : platform(platform)
    {
        platform.RegisterBoardProtection([this](application::PlatformFactory::BoardProtectionReason reason)
            {
                Notify(ToFaultCode(reason));
            });
    }

    void PlatformFaultNotifier::Notify(FaultCode code)
    {
        if (onFaultImmediate != nullptr)
            onFaultImmediate(code);

        infra::EventDispatcher::Instance().Schedule([this, code]()
            {
                if (onFaultDeferred != nullptr)
                    onFaultDeferred(code);
            });

        if (onFaultSecondary != nullptr)
            onFaultSecondary(code);
    }

    void PlatformFaultNotifier::Register(const infra::Function<void(FaultCode)>& onImmediate, const infra::Function<void(FaultCode)>& onDeferred)
    {
        onFaultImmediate = onImmediate;
        onFaultDeferred = onDeferred;

        platform.CanBus().SetOnError([this](application::CanBusAdapter::CanError error)
            {
                if (error == application::CanBusAdapter::CanError::busOff)
                    Notify(FaultCode::hardwareFault);
            });
    }

    void PlatformFaultNotifier::Unregister()
    {
        onFaultImmediate = nullptr;
        onFaultDeferred = nullptr;
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
