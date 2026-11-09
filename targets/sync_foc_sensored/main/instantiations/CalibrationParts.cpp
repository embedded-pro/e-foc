#include "targets/sync_foc_sensored/main/instantiations/CalibrationParts.hpp"

namespace application
{
    namespace
    {
        drivers::EncoderIndex& IndexChannelOr(PlatformFactory& hardware, drivers::EncoderIndex& none)
        {
            const auto channel = hardware.EncoderIndexChannel();
            return channel ? channel->get() : none;
        }
    }

    CalibrationParts::CalibrationParts(PlatformFactory& hardware, DecimatingThreePhaseInverter& inverter, services::InverterCompensation& inverterCompensation)
        : hardware{ hardware }
        , currentOffsets{ inverter, inverterCompensation }
        , confirmedCurrentOffsets{ *currentOffsets }
        , electricalIdent{ inverterCompensation, hardware, [this]()
            {
                return this->hardware.PowerSupplyVoltage();
            } }
        , motorAlignment{ inverterCompensation, hardware }
        , indexSearch{ inverterCompensation, hardware, IndexChannelOr(hardware, noEncoderIndex) }
        , rotorStandstill{ hardware }
    {}
}
