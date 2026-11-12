#include "targets/sync_foc_sensored/main/instantiations/CanServices.hpp"
#include "generated/Version.h"

namespace application
{
    namespace
    {
        can::FocResetCause ToCanResetCause(ResetCause cause)
        {
            switch (cause)
            {
                case ResetCause::brownOut:
                    return can::FocResetCause::brownOut;
                case ResetCause::software:
                    return can::FocResetCause::software;
                case ResetCause::hardware:
                    return can::FocResetCause::hardware;
                case ResetCause::watchdog:
                    return can::FocResetCause::watchdog;
                default:
                    return can::FocResetCause::powerUp;
            }
        }

        can::FocFirmwareIdentity FirmwareIdentityOf(BoardKind boardKind)
        {
            return can::FocFirmwareIdentity{ e_foc::generated::VERSION_MAJOR, e_foc::generated::VERSION_MINOR, e_foc::generated::VERSION_PATCH, static_cast<uint8_t>(boardKind) };
        }
    }

    CanServices::CanServices(const Wiring& wiring)
        : firmwareUpgradeInterlock{ wiring.controlMode }
        , firmwareUpgrade{ wiring.canStack.firmwareUpgradeServer, wiring.hardware.Staging(), firmwareUpgradeInterlock, [&hardware = wiring.hardware]()
            {
                hardware.Reset();
            } }
        , canBridge{ wiring.canStack.motorCanServer, wiring.controlMode, can::FocMotorCanBridge::Platform{ wiring.inverter, wiring.hardware, wiring.hardware.Diagnostics(), ToCanResetCause(wiring.hardware.GetResetCause()), FirmwareIdentityOf(wiring.hardware.GetBoardKind()) }, can::FocMotorCanBridge::Services{ *wiring.calibration.electricalIdent, wiring.calibration.confirmedCurrentOffsets, wiring.calibration.rotorStandstill, firmwareUpgrade, wiring.faultHistory }, wiring.hardware.Tracer() }
        , canLivenessWatchdog{ wiring.canStack.canServer, wiring.controlMode, wiring.hardware.Tracer() }
        , currentStreamer{ wiring.canStack.canServer, wiring.canStack.motorCanServer, wiring.currentStream, wiring.controlMode.Config(), wiring.baseFrequency }
    {}
}
