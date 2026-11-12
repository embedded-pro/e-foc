#pragma once

#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/index_search/IndexSearch.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/SharedPtr.hpp"
#include <chrono>
#include <cstdint>

namespace services
{
    class IndexSearchImpl
        : public IndexSearch
        , public infra::EnableSharedFromThis<IndexSearchImpl>
    {
    public:
        IndexSearchImpl(drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder, drivers::EncoderIndex& index);

        bool Available() const override;
        void Search(const Config& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone) override;
        void Abort() override;
        bool IsRunning() const override;

    private:
        enum class Phase : uint8_t
        {
            idle,
            holding,
            turning,
            found,
            failed
        };

        static bool IsUsable(const Config& config);
        void Prepare(const Config& config);
        void Sample(const foc::PhaseCurrents& currents);
        void Hold();
        void Turn();
        void OutputVector();
        void FailFromInterrupt();
        void Reference();
        void Conclude(std::optional<foc::Radians> reading);

        static constexpr float searchRevolutionsPerSecond{ 0.5f };
        static constexpr float travelRevolutions{ 1.25f };
        static constexpr float tripFactor{ 2.0f };
        static constexpr float maximumShareOfDriveCurrent{ 0.25f };
        static constexpr float maximumDutyFraction{ 0.5f };
        static constexpr std::chrono::milliseconds holdDuration{ 200 };
        static constexpr std::chrono::seconds timeout{ 4 };

        drivers::ThreePhaseInverter& driver;
        drivers::Encoder& encoder;
        drivers::EncoderIndex& index;
        [[no_unique_address]] foc::ClarkePark transforms;
        std::optional<foc::Radians> target;
        float amplitude{ 0.0f };
        foc::Ampere tripCurrent{ 0.0f };
        float stepPerSample{ 0.0f };
        std::size_t holdSamples{ 0 };
        std::size_t travelSamples{ 0 };
        std::size_t samples{ 0 };
        float electricalAngle{ 0.0f };
        // Written by the phase-current interrupt, read by the event loop once it has stopped writing
        Phase phase{ Phase::idle };
        foc::Radians readingAtIndex{ 0.0f };
        uint32_t run{ 0 };
        infra::AutoResetFunction<void(std::optional<foc::Radians>)> onSearchDone;
        infra::TimerSingleShot timeoutTimer;
    };
}
