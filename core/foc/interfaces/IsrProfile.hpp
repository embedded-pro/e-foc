#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace diag
{
    enum class IsrStage : uint8_t
    {
        readout,
        dispatch,
        encoder,
        transform,
        currentLoop,
        modulation,
        estimatorWindow,
        windowClose,
        calculateRest,
        pwm,
        observer,
        total,
        count
    };

    inline constexpr std::array<const char*, static_cast<std::size_t>(IsrStage::count)> isrStageNames{
        "readout", "dispatch", "encoder", "transform", "currentLoop", "modulation",
        "estimatorWindow", "windowClose", "calculateRest", "pwm", "observer", "total"
    };

    class IsrProfile
    {
    public:
        static constexpr std::size_t stages = static_cast<std::size_t>(IsrStage::count);

        void Attach(volatile const uint32_t* cycleCounter)
        {
            counter = cycleCounter;
        }

        void Begin()
        {
            if (counter == nullptr)
                return;
            entry = *counter;
            previous = entry;
        }

        void Mark(IsrStage stage)
        {
            if (counter == nullptr)
                return;
            const auto now = *counter;
            Record(stage, now - previous);
            previous = now;
        }

        void End()
        {
            if (counter == nullptr)
                return;
            Record(IsrStage::total, *counter - entry);
        }

        void Clear()
        {
            maximum = {};
            sum = {};
            hits = {};
        }

        uint32_t Maximum(std::size_t stage) const
        {
            return maximum[stage];
        }

        uint32_t Average(std::size_t stage) const
        {
            return hits[stage] == 0 ? 0 : static_cast<uint32_t>(sum[stage] / hits[stage]);
        }

        uint32_t Hits(std::size_t stage) const
        {
            return hits[stage];
        }

    private:
        void Record(IsrStage stage, uint32_t cycles)
        {
            const auto index = static_cast<std::size_t>(stage);
            ++hits[index];
            sum[index] += cycles;
            if (cycles > maximum[index])
                maximum[index] = cycles;
        }

        volatile const uint32_t* counter{ nullptr };
        uint32_t entry{ 0 };
        uint32_t previous{ 0 };
        std::array<uint32_t, stages> maximum{};
        std::array<uint64_t, stages> sum{};
        std::array<uint32_t, stages> hits{};
    };

    inline IsrProfile isrProfile;
}
