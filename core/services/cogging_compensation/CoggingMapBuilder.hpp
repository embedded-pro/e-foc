#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "core/services/cogging_compensation/CoggingMap.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

namespace services
{
    class CoggingMapBuilder
    {
    public:
        static constexpr std::size_t bins{ coggingMapBins };
        static constexpr uint32_t minimumSamplesPerBin{ 16 };
        // Noise alone agrees to about 1; see documentation/design/service-cogging-compensation.md.
        static constexpr float minimumAgreement{ 3.0f };

        enum class Direction : uint8_t
        {
            forward,
            backward
        };

        void Restart();

        ALWAYS_INLINE_HOT void Add(Direction direction, float electricalAngle, float quadratureCurrent, float mechanicalRotation)
        {
            auto& bin = accumulated[static_cast<std::size_t>(direction)][BinOf(electricalAngle)];
            bin.currentSum += quadratureCurrent;
            bin.rotationSum += mechanicalRotation;
            ++bin.samples;
        }

        std::optional<CoggingMap> Build(foc::NewtonMeter torqueConstant, foc::NewtonMeterSecondSquared inertia, std::size_t polePairs, float samplePeriod) const;

    private:
        struct Bin
        {
            float currentSum{ 0.0f };
            float rotationSum{ 0.0f };
            uint32_t samples{ 0 };
        };

        using Profile = std::array<float, bins>;

        ALWAYS_INLINE_HOT static std::size_t BinOf(float electricalAngle)
        {
            constexpr float perPeriod{ 0.5f * std::numbers::inv_pi_v<float> };
            const auto turns = electricalAngle * perPeriod;
            const auto fraction = turns - std::floor(turns);
            const auto bin = static_cast<std::size_t>(fraction * static_cast<float>(bins));
            return bin < bins ? bin : bins - 1;
        }

        std::optional<Profile> DirectionTorque(Direction direction, float torqueConstant, float inertia, float binWidth, float samplePeriod) const;
        static float Agreement(const Profile& forward, const Profile& backward);

        std::array<std::array<Bin, bins>, 2> accumulated{};
    };
}
