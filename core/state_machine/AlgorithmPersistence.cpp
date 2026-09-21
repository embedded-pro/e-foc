#include "core/state_machine/AlgorithmPersistence.hpp"

namespace state_machine
{
    AlgorithmPersistence::AlgorithmPersistence(services::NonVolatileMemory& nvm, services::ConfigData& configData, services::Tracer& tracer)
        : nvm(nvm)
        , configData(configData)
        , tracer(tracer)
    {}

    void AlgorithmPersistence::ApplyPersistedAlgorithms(foc::CurrentLoopSelectable* current, foc::SpeedLoopSelectable* speed, foc::PositionLoopSelectable* position)
    {
        if (current != nullptr)
        {
            const auto algorithm = CurrentAlgorithmFromRaw(configData.currentAlgorithm);
            if (algorithm.has_value() && *algorithm != current->ActiveCurrentAlgorithm())
                current->SelectCurrentAlgorithm(*algorithm);

            const auto active = current->ActiveCurrentAlgorithm();

            // Record what is running rather than what was asked for: a design that does not
            // converge leaves the previous algorithm active, and a stale record would hide that.
            configData.currentAlgorithm = static_cast<uint8_t>(active);
            tracer.Trace() << "[SM] Current loop algorithm: " << CurrentAlgorithmName(active);
        }

        if (speed != nullptr)
        {
            const auto algorithm = SpeedAlgorithmFromRaw(configData.speedAlgorithm);
            if (algorithm.has_value() && *algorithm != speed->ActiveSpeedAlgorithm())
                speed->SelectSpeedAlgorithm(*algorithm);

            const auto active = speed->ActiveSpeedAlgorithm();

            configData.speedAlgorithm = static_cast<uint8_t>(active);
            tracer.Trace() << "[SM] Speed loop algorithm: " << SpeedAlgorithmName(active);
        }

        if (position != nullptr)
        {
            const auto algorithm = PositionAlgorithmFromRaw(configData.positionAlgorithm);
            if (algorithm.has_value() && *algorithm != position->ActivePositionAlgorithm())
                position->SelectPositionAlgorithm(*algorithm);

            const auto active = position->ActivePositionAlgorithm();

            configData.positionAlgorithm = static_cast<uint8_t>(active);
            tracer.Trace() << "[SM] Position loop algorithm: " << PositionAlgorithmName(active);
        }
    }

    foc::SelectResult AlgorithmPersistence::SelectCurrentAlgorithm(foc::CurrentAlgorithm algorithm, foc::CurrentLoopSelectable* selectable)
    {
        if (selectable == nullptr)
            return foc::SelectResult::invalidAlgorithm;

        auto result = selectable->SelectCurrentAlgorithm(algorithm);
        if (result != foc::SelectResult::ok)
            return result;

        configData.currentAlgorithm = static_cast<uint8_t>(algorithm);
        PersistConfig();
        return result;
    }

    foc::SelectResult AlgorithmPersistence::SelectSpeedAlgorithm(foc::SpeedAlgorithm algorithm, foc::SpeedLoopSelectable* selectable)
    {
        if (selectable == nullptr)
            return foc::SelectResult::invalidAlgorithm;

        auto result = selectable->SelectSpeedAlgorithm(algorithm);
        if (result != foc::SelectResult::ok)
            return result;

        configData.speedAlgorithm = static_cast<uint8_t>(algorithm);
        PersistConfig();
        return result;
    }

    foc::SelectResult AlgorithmPersistence::SelectPositionAlgorithm(foc::PositionAlgorithm algorithm, foc::PositionLoopSelectable* selectable)
    {
        if (selectable == nullptr)
            return foc::SelectResult::invalidAlgorithm;

        auto result = selectable->SelectPositionAlgorithm(algorithm);
        if (result != foc::SelectResult::ok)
            return result;

        configData.positionAlgorithm = static_cast<uint8_t>(algorithm);
        PersistConfig();
        return result;
    }

    foc::CurrentAlgorithm AlgorithmPersistence::ActiveCurrentAlgorithm(const foc::CurrentLoopSelectable* selectable) const
    {
        if (selectable != nullptr)
            return selectable->ActiveCurrentAlgorithm();

        return CurrentAlgorithmFromRaw(configData.currentAlgorithm).value_or(foc::CurrentAlgorithm::pid);
    }

    foc::SpeedAlgorithm AlgorithmPersistence::ActiveSpeedAlgorithm(const foc::SpeedLoopSelectable* selectable) const
    {
        if (selectable != nullptr)
            return selectable->ActiveSpeedAlgorithm();

        return SpeedAlgorithmFromRaw(configData.speedAlgorithm).value_or(foc::SpeedAlgorithm::pid);
    }

    foc::PositionAlgorithm AlgorithmPersistence::ActivePositionAlgorithm(const foc::PositionLoopSelectable* selectable) const
    {
        if (selectable != nullptr)
            return selectable->ActivePositionAlgorithm();

        return PositionAlgorithmFromRaw(configData.positionAlgorithm).value_or(foc::PositionAlgorithm::pid);
    }

    const char* AlgorithmPersistence::CurrentAlgorithmName(foc::CurrentAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case foc::CurrentAlgorithm::pid:
                return "pid";
            case foc::CurrentAlgorithm::decoupledPid:
                return "decoupled";
            case foc::CurrentAlgorithm::deadbeat:
                return "deadbeat";
            case foc::CurrentAlgorithm::slidingMode:
                return "sliding";
            default:
                return "unknown";
        }
    }

    const char* AlgorithmPersistence::SpeedAlgorithmName(foc::SpeedAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case foc::SpeedAlgorithm::pid:
                return "pid";
            case foc::SpeedAlgorithm::lqi:
                return "lqi";
            case foc::SpeedAlgorithm::adrc:
                return "adrc";
            case foc::SpeedAlgorithm::twoDof:
                return "twodof";
            default:
                return "unknown";
        }
    }

    const char* AlgorithmPersistence::PositionAlgorithmName(foc::PositionAlgorithm algorithm)
    {
        switch (algorithm)
        {
            case foc::PositionAlgorithm::pid:
                return "pid";
            case foc::PositionAlgorithm::cascadeP:
                return "cascadep";
            case foc::PositionAlgorithm::lqr:
                return "lqr";
            case foc::PositionAlgorithm::lqi:
                return "lqi";
            case foc::PositionAlgorithm::twoDof:
                return "twodof";
            default:
                return "unknown";
        }
    }

    void AlgorithmPersistence::PersistConfig()
    {
        nvm.SaveConfig(configData, [this](services::NvmStatus status)
            {
                if (status != services::NvmStatus::Ok)
                    tracer.Trace() << "config persist failed";
            });
    }

    std::optional<foc::CurrentAlgorithm> AlgorithmPersistence::CurrentAlgorithmFromRaw(uint8_t raw)
    {
        if (raw > static_cast<uint8_t>(foc::CurrentAlgorithm::slidingMode))
            return std::nullopt;
        return static_cast<foc::CurrentAlgorithm>(raw);
    }

    std::optional<foc::SpeedAlgorithm> AlgorithmPersistence::SpeedAlgorithmFromRaw(uint8_t raw)
    {
        if (raw > static_cast<uint8_t>(foc::SpeedAlgorithm::twoDof))
            return std::nullopt;
        return static_cast<foc::SpeedAlgorithm>(raw);
    }

    std::optional<foc::PositionAlgorithm> AlgorithmPersistence::PositionAlgorithmFromRaw(uint8_t raw)
    {
        if (raw > static_cast<uint8_t>(foc::PositionAlgorithm::twoDof))
            return std::nullopt;
        return static_cast<foc::PositionAlgorithm>(raw);
    }
}
