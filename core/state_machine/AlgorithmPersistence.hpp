#pragma once

#include "core/foc/interfaces/Algorithms.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/services/non_volatile_memory/ConfigData.hpp"
#include "core/services/non_volatile_memory/NonVolatileMemory.hpp"
#include "services/tracer/Tracer.hpp"
#include <optional>

namespace state_machine
{
    class AlgorithmPersistence
    {
    public:
        AlgorithmPersistence(services::NonVolatileMemory& nvm, services::ConfigData& configData, services::Tracer& tracer);

        void ApplyPersistedAlgorithms(
            foc::CurrentLoopSelectable* current,
            foc::SpeedLoopSelectable* speed,
            foc::PositionLoopSelectable* position);

        foc::SelectResult SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm, foc::CurrentLoopSelectable* selectable);
        foc::SelectResult SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm, foc::SpeedLoopSelectable* selectable);
        foc::SelectResult SelectPositionAlgorithm(foc::PositionAlgorithm algorithm, foc::PositionLoopSelectable* selectable);

        foc::CurrentAlgorithm ActiveCurrentAlgorithm(const foc::CurrentLoopSelectable* selectable) const;
        foc::SpeedAlgorithm ActiveSpeedAlgorithm(const foc::SpeedLoopSelectable* selectable) const;
        foc::PositionAlgorithm ActivePositionAlgorithm(const foc::PositionLoopSelectable* selectable) const;

        static const char* CurrentAlgorithmName(foc::CurrentAlgorithm algorithm);
        static const char* SpeedAlgorithmName(foc::SpeedAlgorithm algorithm);
        static const char* PositionAlgorithmName(foc::PositionAlgorithm algorithm);

    private:
        void PersistConfig();

        static std::optional<foc::CurrentAlgorithm> CurrentAlgorithmFromRaw(uint8_t raw);
        static std::optional<foc::SpeedAlgorithm> SpeedAlgorithmFromRaw(uint8_t raw);
        static std::optional<foc::PositionAlgorithm> PositionAlgorithmFromRaw(uint8_t raw);

        services::NonVolatileMemory& nvm;
        services::ConfigData& configData;
        services::Tracer& tracer;
    };
}
