#pragma once

#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/alignment/MotorAlignment.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/SharedPtr.hpp"
#include <cstdint>

namespace services
{
    class MotorAlignmentImpl
        : public MotorAlignment
        , public infra::EnableSharedFromThis<MotorAlignmentImpl>
    {
    public:
        MotorAlignmentImpl(drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder);

        void ForceAlignment(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone) override;
        void Abort() override;
        Failure LastFailure() const override;

    private:
        void ApplyAlignmentVoltage();
        void OutputVector(float electricalAngle);
        void BeginFinalHold();
        bool CountsSettlement() const;
        bool FollowedField() const;
        void CalculateAlignmentOffset();
        void ProcessPosition(const foc::PhaseCurrents& currents);
        void Settle();
        bool CarriesNoCurrent() const;
        std::optional<float> ExpectedPhaseACurrent() const;
        void FailToConverge();
        void Finish(std::optional<foc::Radians> result);
        void Complete(std::optional<foc::Radians> result);

        constexpr static uint8_t neutralDuty = 50;
        constexpr static float alignmentAngle = 0.0f;
        constexpr static float missingFraction = 0.25f;
        constexpr static float minimumFollow = 0.5f;
        constexpr static float maximumFollow = 1.5f;

        drivers::ThreePhaseInverter& driver;
        drivers::Encoder& encoder;
        [[no_unique_address]] foc::ClarkePark transforms;
        AlignmentConfig alignmentConfig;
        std::size_t polePairs = 1;
        std::size_t currentSampleIndex = 0;
        std::size_t consecutiveSettledSamples = 0;
        foc::Radians previousPosition{ 0.0f };
        foc::Radians alignedPosition{ 0.0f };
        float settledSumA{ 0.0f };
        float settledSumB{ 0.0f };
        float settledSumC{ 0.0f };
        Failure failure{ Failure::none };
        bool firstHold{ false };
        std::size_t samplesInHold = 0;
        foc::Radians firstHoldPosition{ 0.0f };
        std::optional<foc::Radians> outcome;
        infra::AutoResetFunction<void(std::optional<foc::Radians>)> onAlignmentDone;
        infra::TimerSingleShot timeoutTimer;
        uint32_t run{ 0 };
        bool finishing{ false };
    };
}
