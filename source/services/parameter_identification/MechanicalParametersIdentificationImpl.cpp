#include "source/services/parameter_identification/MechanicalParametersIdentificationImpl.hpp"
#include "source/foc/instantiations/TrigonometricImpl.hpp"
#include "source/foc/interfaces/Units.hpp"
#include <cmath>
#include <numbers>

namespace
{
    const hal::Hertz samplingFrequency{ 10000 };
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float twoPi = 2.0f * pi;

}

namespace services
{
    MechanicalParametersIdentificationImpl::MechanicalParametersIdentificationImpl(foc::SpeedController& controller, foc::MotorDriver& driver, foc::Encoder& encoder)
        : controller(controller)
        , driver(driver)
        , encoder(encoder)
        , samplingPeriod(1.0f / static_cast<float>(samplingFrequency.Value()))
    {
    }

    void MechanicalParametersIdentificationImpl::EstimateFrictionAndInertia(const foc::NewtonMeter& torqueConstant, std::size_t numberOfPolePairs, const Config& config, const infra::Function<void(std::optional<foc::NewtonMeterSecondPerRadian>, std::optional<foc::NewtonMeterSecondSquared>)>& onDone)
    {
        this->currentConfig = config;
        this->onDone = onDone;
        this->previousPosition = encoder.Read().Value();
        this->previousSpeed = 0.0f;
        this->polePairs = static_cast<float>(numberOfPolePairs);

        rls.emplace(1000.0f, config.forgettingFactor);

        controller.Enable();
        controller.SetPoint(config.targetSpeed);

        driver.PhaseCurrentsReady(hal::Hertz{ 10000 }, [this, torqueConstant](auto currents)
            {
                OnSamplingUpdate(currents, torqueConstant);
            });

        timeoutTimer.Start(config.timeout, [this]()
            {
                driver.PhaseCurrentsReady(hal::Hertz{ 10000 }, [](auto) {});
                this->onDone(std::nullopt, std::nullopt);
            });
    }

    void MechanicalParametersIdentificationImpl::OnSamplingUpdate(foc::PhaseCurrents currents, const foc::NewtonMeter& torqueConstant)
    {
        auto mechanicalPos = encoder.Read().Value();
        auto electricalAngle = mechanicalPos * polePairs;
        auto rotatingFrame = transform.Forward(foc::ThreePhase{ currents.a.Value(), currents.b.Value(), currents.c.Value() }, foc::FastTrigonometry::Cosine(electricalAngle), foc::FastTrigonometry::Sine(electricalAngle));
        auto speed = (encoder.Read().Value() - previousPosition) / samplingPeriod;
        auto acceleration = (speed - previousSpeed) / samplingPeriod;
        MotorRLS::MakeRegressor(regressor, acceleration, speed);

        torque.at(0, 0) = rotatingFrame.q * torqueConstant.Value();

        auto metrics = rls->Update(regressor, torque);

        if (MotorRLS::EvaluateConvergence(metrics, 1e-4f, 1e-2f) == estimators::State::converged)
        {
            timeoutTimer.Cancel();
            driver.PhaseCurrentsReady(hal::Hertz{ 10000 }, [](auto) {});

            auto& theta = rls->Coefficients();
            onDone(
                foc::NewtonMeterSecondPerRadian{ theta.at(2, 0) },
                foc::NewtonMeterSecondSquared{ theta.at(1, 0) });
        }

        previousPosition = mechanicalPos;
        previousSpeed = speed;
    }
}
