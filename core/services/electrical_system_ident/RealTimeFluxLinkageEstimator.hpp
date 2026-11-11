#pragma once

#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include <cstdint>

namespace services
{
    // Scalar least squares through the origin on the quadrature voltage equation, with the resistive, inductive and cross-coupling terms taken off the measured voltage.
    class RealTimeFluxLinkageEstimator
        : public foc::OnlineFluxLinkageEstimator
    {
    public:
        static constexpr float defaultForgettingFactor = 0.9995f;
        static constexpr float minimumBackEmfFraction = 0.02f;
        static constexpr uint16_t minimumUpdates = 200;

        explicit RealTimeFluxLinkageEstimator(float forgettingFactor, hal::Hertz outerFrequency);

        void SetInitialEstimate(foc::Weber fluxLinkage) override;

        void Update(const foc::QuadratureWindow& window) override;

        foc::Weber CurrentFluxLinkage() const override;
        bool HasSettled() const override;

    private:
        bool Informative(const foc::QuadratureWindow& window, float speed) const;
        void Publish();

        float forgettingFactor;
        float outerFrequency;
        float previousIqAtEnd{ 0.0f };
        float crossCorrelation{ 0.0f };
        float autoCorrelation{ 0.0f };
        float seed{ 0.0f };
        float published{ 0.0f };
        bool settled{ false };
        uint16_t updates{ 0 };
    };
}
