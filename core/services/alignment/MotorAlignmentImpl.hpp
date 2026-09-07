#pragma once

#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "core/platform_abstraction/interfaces/Drivers.hpp"
#include "core/services/alignment/MotorAlignment.hpp"
#include "infra/util/AutoResetFunction.hpp"

namespace services
{
    class MotorAlignmentImpl
        : public MotorAlignment
    {
    public:
        MotorAlignmentImpl(drivers::ThreePhaseInverter& driver, drivers::Encoder& encoder);

        void ForceAlignment(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone) override;
        void Abort() override;

    private:
        void ApplyAlignmentVoltage();
        void CalculateAlignmentOffset();
        void ProcessPosition();
        void FailToConverge();

        constexpr static uint8_t neutralDuty = 50;
        constexpr static float alignmentAngle = 0.0f;

        drivers::ThreePhaseInverter& driver;
        drivers::Encoder& encoder;
        [[no_unique_address]] foc::ClarkePark transforms;
        AlignmentConfig alignmentConfig;
        std::size_t polePairs = 1;
        std::size_t currentSampleIndex = 0;
        std::size_t consecutiveSettledSamples = 0;
        foc::Radians previousPosition{ 0.0f };
        foc::Radians alignedPosition{ 0.0f };
        infra::AutoResetFunction<void(std::optional<foc::Radians>)> onAlignmentDone;
    };
}
