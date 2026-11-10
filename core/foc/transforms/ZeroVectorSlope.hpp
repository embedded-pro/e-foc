#pragma once

#include "core/foc/interfaces/MotorModel.hpp"
#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/transforms/TransformsClarkePark.hpp"
#include "numerical/math/CompilerOptimizations.hpp"
#include "numerical/math/Math.hpp"

namespace foc
{
    class ZeroVectorSlope
    {
    public:
        void Configure(const MotorModelParameters& parameters)
        {
            const auto frequency = static_cast<float>(parameters.samplingFrequency.Value());
            model = Model{ parameters.resistance.Value(), parameters.inductance.Value(), parameters.fluxLinkage.Value() };
            samplePeriod = frequency > 0.0f ? 1.0f / frequency : 0.0f;

            if (!model.Usable() || samplePeriod == 0.0f)
                model = Model{};

            Restart();
        }

        void Restart()
        {
            Publish(model.ToGains());
        }

        void Track(Ohm resistance, MilliHenry inductance)
        {
            if (!model.Usable() || !Within(resistanceEstimateBand, resistance.Value(), model.resistance) || !Within(inductanceEstimateBand, inductance.Value(), model.inductanceMilliHenry))
                return;

            Publish(Model{ resistance.Value(), inductance.Value(), model.fluxLinkage }.ToGains());
        }

        OPTIMIZE_FOR_SPEED PhaseCurrentSlopes Next(const RotatingFrame& current, float electricalSpeed, float cosTheta, float sinTheta)
        {
            const float decay = resistanceOverInductance;
            const float d = -decay * current.d;
            const float q = -decay * current.q - electricalSpeed * fluxOverInductance;
            const float advance = electricalSpeed * samplePeriod;
            const float keep = 1.0f - 0.5f * advance * advance;
            const auto phases = clarke.Inverse(park.Inverse(RotatingFrame{ keep * d - advance * q, keep * q + advance * d }, cosTheta, sinTheta));

            return { AmperePerSecond{ phases.a }, AmperePerSecond{ phases.b }, AmperePerSecond{ phases.c } };
        }

    private:
        struct Gains
        {
            float resistanceOverInductance{ 0.0f };
            float fluxOverInductance{ 0.0f };
        };

        struct Model
        {
            float resistance{ 0.0f };
            float inductanceMilliHenry{ 0.0f };
            float fluxLinkage{ 0.0f };

            bool Usable() const
            {
                return math::IsFinite(resistance) && math::IsFinite(inductanceMilliHenry) && math::IsFinite(fluxLinkage) && inductanceMilliHenry > 0.0f;
            }

            Gains ToGains() const
            {
                if (!Usable())
                    return {};

                constexpr float henryPerMilliHenry{ 0.001f };
                const auto inductance = inductanceMilliHenry * henryPerMilliHenry;
                return { resistance / inductance, fluxLinkage / inductance };
            }
        };

        void Publish(const Gains& gains)
        {
            resistanceOverInductance = gains.resistanceOverInductance;
            fluxOverInductance = gains.fluxOverInductance;
        }

        static bool Within(const EstimateBand& band, float value, float reference)
        {
            const auto ratio = value / reference;
            return math::IsFinite(ratio) && ratio >= band.minimum && ratio <= band.maximum;
        }

        [[no_unique_address]] Clarke clarke;
        [[no_unique_address]] Park park;
        Model model;
        float samplePeriod{ 0.0f };
        volatile float resistanceOverInductance{ 0.0f };
        volatile float fluxOverInductance{ 0.0f };
    };
}
