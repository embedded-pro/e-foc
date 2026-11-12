#pragma once

#include "core/services/index_search/IndexSearch.hpp"
#include "core/services/non_volatile_memory/CalibrationData.hpp"
#include "e-foc-protocol/foc/Units.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>
#include <functional>
#include <optional>

namespace application
{
    class IndexReference
    {
    public:
        using Search = std::optional<std::reference_wrapper<services::IndexSearch>>;

        IndexReference(Search search, services::Tracer& tracer);

        bool CanRestore(const services::CalibrationData& record) const;
        void Start(const services::CalibrationData& record, foc::Volts busVoltage, const infra::Function<void(bool)>& onDone);
        bool IsRunning() const;
        void Abort();

    private:
        void Conclude(std::optional<foc::Radians> reading);

        Search search;
        services::Tracer& tracer;
        infra::AutoResetFunction<void(bool)> onDone;
        uint32_t run{ 0 };
    };
}
