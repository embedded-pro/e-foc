#pragma once

#include "numerical/math/CompilerOptimizations.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace foc
{
    struct PlantResponseSample
    {
        uint32_t tick{ 0 };
        float omegaMech{ 0.0f };
        float thetaMech{ 0.0f };
        float iq{ 0.0f };
        float id{ 0.0f };
        float externalTorqueNm{ 0.0f };
    };

    enum class PlantResponseKind : uint8_t
    {
        sample,
        began,
        torqueStep,
        stopped
    };

    struct PlantResponseRecord
    {
        PlantResponseKind kind{ PlantResponseKind::sample };
        uint32_t dropped{ 0 };
        PlantResponseSample sample{};
    };

    // Single producer (the control tick) and single consumer (the event loop). Begin/End only raise
    // flags so the tick stays the only writer of the ring. Markers never enter the ring: each has
    // its own slot the tick fills and the consumer empties, so a full ring can drop samples but
    // never the record that reports the drops.
    template<std::size_t Capacity>
    class PlantResponseRecorder
    {
    public:
        void Configure(uint32_t decimation, uint32_t maxSamples);
        bool IsEnabled() const;

        void Begin();
        void End();

        OPTIMIZE_FOR_SPEED void Capture(const PlantResponseSample& sample);
        OPTIMIZE_FOR_SPEED void Note(PlantResponseKind kind, const PlantResponseSample& sample);

        std::optional<PlantResponseRecord> Pop();
        uint32_t Dropped() const;

    private:
        struct MarkerSlot
        {
            std::atomic<bool> pending{ false };
            PlantResponseRecord record{};
        };

        void Push(const PlantResponseRecord& record);
        void Post(MarkerSlot& slot, const PlantResponseRecord& record);
        std::optional<PlantResponseRecord> Take(MarkerSlot& slot);
        void ServicePendingMarkers(const PlantResponseSample& sample);

    private:
        std::array<PlantResponseRecord, Capacity> ring{};
        std::atomic<uint32_t> head{ 0 };
        std::atomic<uint32_t> tail{ 0 };
        std::atomic<bool> pendingBegin{ false };
        std::atomic<bool> pendingEnd{ false };
        MarkerSlot began;
        MarkerSlot event;
        MarkerSlot stopped;
        uint32_t decimation{ 0 };
        uint32_t maxSamples{ 0 };
        uint32_t emitted{ 0 };
        uint32_t phase{ 0 };
        std::atomic<uint32_t> dropped{ 0 };
        bool armed{ false };
    };

    template<std::size_t Capacity>
    void PlantResponseRecorder<Capacity>::Configure(uint32_t decimation, uint32_t maxSamples)
    {
        this->decimation = decimation;
        this->maxSamples = maxSamples;
    }

    template<std::size_t Capacity>
    bool PlantResponseRecorder<Capacity>::IsEnabled() const
    {
        return decimation != 0;
    }

    template<std::size_t Capacity>
    void PlantResponseRecorder<Capacity>::Begin()
    {
        if (IsEnabled())
            pendingBegin.store(true, std::memory_order_release);
    }

    template<std::size_t Capacity>
    void PlantResponseRecorder<Capacity>::End()
    {
        if (IsEnabled())
            pendingEnd.store(true, std::memory_order_release);
    }

    template<std::size_t Capacity>
    void PlantResponseRecorder<Capacity>::ServicePendingMarkers(const PlantResponseSample& sample)
    {
        if (pendingBegin.exchange(false, std::memory_order_acq_rel))
        {
            emitted = 0;
            phase = 0;
            dropped.store(0, std::memory_order_relaxed);
            armed = true;
            Post(began, { PlantResponseKind::began, 0, sample });
        }

        if (pendingEnd.exchange(false, std::memory_order_acq_rel) && armed)
        {
            armed = false;
            Post(stopped, { PlantResponseKind::stopped, dropped.load(std::memory_order_relaxed), sample });
        }
    }

    template<std::size_t Capacity>
    void PlantResponseRecorder<Capacity>::Capture(const PlantResponseSample& sample)
    {
        if (!IsEnabled())
            return;

        ServicePendingMarkers(sample);

        if (!armed)
            return;

        if (phase != 0)
        {
            --phase;
            return;
        }

        phase = decimation - 1;

        if (maxSamples != 0 && emitted >= maxSamples)
            return;

        ++emitted;
        Push({ PlantResponseKind::sample, 0, sample });
    }

    template<std::size_t Capacity>
    void PlantResponseRecorder<Capacity>::Note(PlantResponseKind kind, const PlantResponseSample& sample)
    {
        if (!IsEnabled() || !armed)
            return;

        Post(event, { kind, 0, sample });
    }

    template<std::size_t Capacity>
    void PlantResponseRecorder<Capacity>::Push(const PlantResponseRecord& record)
    {
        const uint32_t currentHead = head.load(std::memory_order_relaxed);
        const uint32_t next = (currentHead + 1) % Capacity;

        if (next == tail.load(std::memory_order_acquire))
        {
            dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        ring[currentHead] = record;
        head.store(next, std::memory_order_release);
    }

    template<std::size_t Capacity>
    void PlantResponseRecorder<Capacity>::Post(MarkerSlot& slot, const PlantResponseRecord& record)
    {
        if (slot.pending.load(std::memory_order_acquire))
        {
            dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        slot.record = record;
        slot.pending.store(true, std::memory_order_release);
    }

    template<std::size_t Capacity>
    std::optional<PlantResponseRecord> PlantResponseRecorder<Capacity>::Take(MarkerSlot& slot)
    {
        if (!slot.pending.load(std::memory_order_acquire))
            return std::nullopt;

        const PlantResponseRecord record = slot.record;
        slot.pending.store(false, std::memory_order_release);
        return record;
    }

    template<std::size_t Capacity>
    std::optional<PlantResponseRecord> PlantResponseRecorder<Capacity>::Pop()
    {
        if (auto record = Take(began))
            return record;

        if (auto record = Take(event))
            return record;

        const uint32_t currentTail = tail.load(std::memory_order_relaxed);

        if (currentTail != head.load(std::memory_order_acquire))
        {
            const PlantResponseRecord record = ring[currentTail];
            tail.store((currentTail + 1) % Capacity, std::memory_order_release);
            return record;
        }

        return Take(stopped);
    }

    template<std::size_t Capacity>
    uint32_t PlantResponseRecorder<Capacity>::Dropped() const
    {
        return dropped.load(std::memory_order_relaxed);
    }
}
