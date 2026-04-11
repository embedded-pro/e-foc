#pragma once

#include "core/services/electrical_system_ident/ElectricalParametersIdentification.hpp"
#include <gmock/gmock.h>

namespace services
{
    class ElectricalParametersIdentificationMock
        : public ElectricalParametersIdentification
    {
    public:
        MOCK_METHOD(void, EstimateResistanceAndInductance,
            (const ResistanceAndInductanceConfig& config,
                const infra::Function<void(std::optional<foc::Ohm>, std::optional<foc::MilliHenry>)>& onDone),
            (override));
        MOCK_METHOD(void, EstimateNumberOfPolePairs,
            (const PolePairsConfig& config,
                const infra::Function<void(std::optional<std::size_t>)>& onDone),
            (override));
    };
}
