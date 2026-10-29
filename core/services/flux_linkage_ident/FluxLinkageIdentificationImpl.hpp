#pragma once

#include "core/foc/interfaces/Execution.hpp"
#include "core/foc/interfaces/Foc.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/flux_linkage_ident/BackEmfFit.hpp"
#include "core/services/flux_linkage_ident/FluxLinkageIdentification.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include <cstdint>

namespace services
{
    class FluxLinkageIdentificationImpl
        : public FluxLinkageIdentification
    {
    public:
        static constexpr float spinCurrentFraction{ 0.25f };

        FluxLinkageIdentificationImpl(foc::CurrentCommandable& controller, foc::Controllable& drive, const foc::FocBase& motion, drivers::ThreePhaseInverter& inverter);

        void Identify(std::size_t polePairs, foc::Ohm resistance, const Config& config, const infra::Function<void(const Result& result)>& onDone) override;
        bool IsRunning() const override;
        void Abort() override;

    private:
        enum class Phase : uint8_t
        {
            idle,
            accelerating,
            settling,
            recording,
            slowing,
            braking
        };

        void Tick();
        void Accelerate();
        void Settle();
        void StartRecording();
        void Record(float speed, const foc::MotionObservation& observation);
        void AddPoint();
        void EndCoast(float speed);
        void Slow(float speed);
        void Brake(float speed);
        bool SpansSpeeds() const;
        void StartBraking();
        void Finish(const Result& result);
        void StopMotion();
        Result Evaluate() const;
        uint32_t TicksIn(infra::Duration duration) const;

        foc::CurrentCommandable& controller;
        foc::Controllable& drive;
        const foc::FocBase& motion;
        foc::Ampere supportedCurrent;

        Config config;
        infra::AutoResetFunction<void(const Result& result)> onDone;
        BackEmfFit fit;
        Phase phase{ Phase::idle };
        float polePairs{ 0.0f };
        float resistance{ 0.0f };
        float spinCurrent{ 0.0f };
        float recordingCeiling{ 0.0f };
        float previousPointSpeed{ 0.0f };
        uint32_t phaseTicks{ 0 };
        uint8_t coasts{ 0 };
        float pointSpeed{ 0.0f };
        float pointVoltage{ 0.0f };
        uint32_t pointTicks{ 0 };
        infra::TimerRepeating tickTimer;
        infra::TimerSingleShot timeoutTimer;
    };
}
