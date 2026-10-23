#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/standstill/RotorStandstill.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include <chrono>
#include <cstddef>

namespace services
{
    class RotorStandstillImpl
        : public RotorStandstill
    {
    public:
        static constexpr std::chrono::milliseconds pollPeriod{ 10 };
        static constexpr float bandRadians{ 0.002f };
        static constexpr std::size_t pollsAtRest{ 10 };
        static constexpr std::chrono::seconds timeout{ 10 };

        explicit RotorStandstillImpl(drivers::Encoder& encoder);

        void Await(const infra::Function<void(bool atRest)>& onDone) override;
        void Abort() override;

    private:
        void Poll();
        void Finish(bool atRest);

        drivers::Encoder& encoder;
        infra::AutoResetFunction<void(bool)> onDone;
        infra::TimerRepeating pollTimer;
        infra::TimerSingleShot timeoutTimer;
        foc::Radians anchor{ 0.0f };
        std::size_t pollsInBand{ 0 };
    };
}
