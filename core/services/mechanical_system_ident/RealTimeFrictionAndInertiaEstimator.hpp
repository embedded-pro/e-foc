#pragma once

#include "core/foc/interfaces/OnlineEstimators.hpp"
#include "core/foc/interfaces/Signals.hpp"
#include "core/foc/interfaces/Units.hpp"
#include "core/services/mechanical_system_ident/MechanicalEstimatePolicy.hpp"
#include "core/services/mechanical_system_ident/MechanicalWindowKinematics.hpp"
#include "core/services/mechanical_system_ident/SteadySpeedFrictionFit.hpp"
#include "numerical/estimators/online/RecursiveLeastSquares.hpp"
#include <array>
#include <optional>

namespace services
{
    class RealTimeFrictionAndInertiaEstimator
        : public foc::OnlineMechanicalEstimator
    {
    public:
        static constexpr float defaultForgettingFactor = 0.9995f;

        RealTimeFrictionAndInertiaEstimator(float forgettingFactor, hal::Hertz samplingFrequency);

        void SetTorqueConstant(foc::NewtonMeter kt) override;
        void SetInitialEstimate(foc::NewtonMeterSecondSquared inertia, foc::NewtonMeterSecondPerRadian friction) override;

        void Update(const foc::MechanicalWindow& window) override;

        foc::NewtonMeterSecondSquared CurrentInertia() const override;
        foc::NewtonMeterSecondPerRadian CurrentFriction() const override;

    private:
        using MotorRLS = MechanicalRls;

        struct DirectionalFit
        {
            std::optional<MotorRLS> rls;
            SteadySpeedFrictionFit steady;
            MotorRLS::EstimationMetrics metrics{};
            uint16_t periods{ 0 };
            MechanicalExcitation excitation;
            float residualPower{ 0.0f };
            float outputPower{ 0.0f };
        };

        void ReseedOnline();
        bool IsOnlineObservationInDirection(float acceleration, float speed) const;
        bool IsOnlineObservationInformative(float acceleration, const DirectionalFit& fit) const;
        bool HasOnlineEstimateSettled(const DirectionalFit& fit) const;
        void PublishOnlineEstimate(const DirectionalFit& fit);
        std::optional<float> SteadyFriction() const;

        float samplingFrequency;
        float forgettingFactor;
        std::array<DirectionalFit, 2> onlineFits;
        MotorRLS::InputMatrix onlineRegressor;
        MechanicalWindowKinematics kinematics;

        foc::NewtonMeter torqueConstant{ 1.0f };
        foc::NewtonMeterSecondSquared currentInertia{ 0.0f };
        foc::NewtonMeterSecondPerRadian currentFriction{ 0.0f };
    };
}
