#pragma once

#include "core/platform_abstraction/DecimatingThreePhaseInverter.hpp"
#include "core/platform_abstraction/NoEncoderIndex.hpp"
#include "core/platform_abstraction/PlatformFactory.hpp"
#include "core/services/alignment/MotorAlignmentImpl.hpp"
#include "core/services/current_offsets/ConfirmedPhaseCurrentOffsets.hpp"
#include "core/services/current_offsets/PhaseCurrentOffsetMeasurement.hpp"
#include "core/services/electrical_system_ident/ElectricalParametersIdentificationImpl.hpp"
#include "core/services/index_search/IndexSearchImpl.hpp"
#include "core/services/inverter_compensation/InverterCompensation.hpp"
#include "core/services/standstill/RotorStandstillImpl.hpp"
#include "infra/util/WithSharedAccess.hpp"

namespace application
{
    struct CalibrationParts
    {
        CalibrationParts(PlatformFactory& hardware, DecimatingThreePhaseInverter& inverter, services::InverterCompensation& inverterCompensation);

        PlatformFactory& hardware;
        infra::WithSharedAccess<services::PhaseCurrentOffsetMeasurement> currentOffsets;
        services::ConfirmedPhaseCurrentOffsets confirmedCurrentOffsets;
        infra::WithSharedAccess<services::ElectricalParametersIdentificationImpl> electricalIdent;
        infra::WithSharedAccess<services::MotorAlignmentImpl> motorAlignment;
        NoEncoderIndex noEncoderIndex;
        infra::WithSharedAccess<services::IndexSearchImpl> indexSearch;
        services::RotorStandstillImpl rotorStandstill;
    };
}
