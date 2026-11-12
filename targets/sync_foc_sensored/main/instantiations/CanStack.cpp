#include "targets/sync_foc_sensored/main/instantiations/CanStack.hpp"
#include "core/can/FocMotorCurrentStreamer.hpp"

namespace application
{
    CanStack::CanStack(PlatformFactory& hardware, uint16_t nodeId)
        : tracingCan{ hardware.CanBus(), hardware.Tracer() }
        , canBus{ tracingCan, hardware.CanBus(), [](hal::Can::Id id)
            {
                return can::FocMotorCurrentStreamer::IsStreamFrame(id);
            } }
        , canServer{ canBus, services::CanProtocolServer::Config{ .nodeId = nodeId } }
        , tracingServerObserver{ canServer, hardware.Tracer() }
        , motorCanServer{ canServer.Transport() }
        , firmwareUpgradeServer{ canServer.Transport() }
    {
        canServer.RegisterCategory(motorCanServer);
        canServer.RegisterCategory(firmwareUpgradeServer);
    }
}
