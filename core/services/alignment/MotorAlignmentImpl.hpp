#pragma once

#include "infra/util/AutoResetFunction.hpp"
#include "core/foc/implementations/TransformsClarkePark.hpp"
#include "core/foc/interfaces/Driver.hpp"
#include "core/services/alignment/MotorAlignment.hpp"

namespace services
{
    class MotorAlignmentImpl
        : public MotorAlignment
    {
    public:
        MotorAlignmentImpl(foc::ThreePhaseInverter& driver, foc::Encoder& encoder);

        void ForceAlignment(std::size_t polePairs, const AlignmentConfig& config, const infra::Function<void(std::optional<foc::Radians>)>& onDone) override;

    private:
        void ApplyAlignmentVoltage();
        void CalculateAlignmentOffset();
        void ProcessPosition();
        void FailToConverge();

        constexpr static uint8_t neutralDuty = 50;
        constexpr static float alignmentAngle = 0.0f;

        foc::ThreePhaseInverter& driver;
        foc::Encoder& encoder;
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
