#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "tools/simulator/model/Model.hpp"
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <cstddef>
#include <optional>

namespace simulator
{
    class ParametersPanel
        : public QWidget
    {
        Q_OBJECT

    public:
        struct LoopPid
        {
            float kp;
            float ki;
            float kd;
        };

        struct PidParameters
        {
            LoopPid current;
            std::optional<LoopPid> speed;
            std::optional<LoopPid> position;
        };

        ParametersPanel(const ThreePhaseMotorModel::Parameters& motorParameters, const PidParameters& pidParameters, QWidget* parent = nullptr);

        void UpdatePidParameters(const PidParameters& pidParameters);
        void UpdateResistance(foc::Ohm value);
        void UpdateInductance(foc::MilliHenry value);
        void UpdateFriction(foc::NewtonMeterSecondPerRadian value);
        void UpdateInertia(foc::NewtonMeterSecondSquared value);
        void UpdatePolePairs(std::size_t value);
        void UpdateAlignmentOffset(foc::Radians value);
        void UpdateLiveThermal(float tempCelsius, foc::Ohm rEff, foc::Henry lEff);

    signals:
        void noiseConfigChanged(simulator::ThreePhaseMotorModel::NoiseConfig config);
        void encoderNoiseConfigChanged(simulator::ThreePhaseMotorModel::EncoderNoiseConfig config);
        void thermalConfigChanged(simulator::ThreePhaseMotorModel::ThermalConfig config);
        void thermalResetRequested();

    private:
        void EmitNoiseConfig();
        void EmitEncoderNoiseConfig();
        void EmitThermalConfig();

    private:
        QLabel* resistanceLabel;
        QLabel* inductanceLabel;
        QLabel* frictionLabel;
        QLabel* inertiaLabel;
        QLabel* polePairsLabel;
        QLabel* alignmentOffsetLabel;

        QLabel* currentKpLabel;
        QLabel* currentKiLabel;
        QLabel* speedKpLabel = nullptr;
        QLabel* speedKiLabel = nullptr;
        QLabel* speedKdLabel = nullptr;
        QLabel* positionKpLabel = nullptr;
        QLabel* positionKiLabel = nullptr;
        QLabel* positionKdLabel = nullptr;

        QDoubleSpinBox* sigmaSpin{ nullptr };
        QDoubleSpinBox* biasASpin{ nullptr };
        QDoubleSpinBox* biasBSpin{ nullptr };
        QDoubleSpinBox* biasCSpin{ nullptr };

        QDoubleSpinBox* encoderSigmaSpin{ nullptr };
        QDoubleSpinBox* encoderBiasSpin{ nullptr };

        QDoubleSpinBox* tAmbientSpin{ nullptr };
        QDoubleSpinBox* rThSpin{ nullptr };
        QDoubleSpinBox* cThSpin{ nullptr };
        QDoubleSpinBox* alphaCuSpin{ nullptr };
        QDoubleSpinBox* betaFeSpin{ nullptr };
        QPushButton* resetTempButton{ nullptr };

        QLabel* tWindingLabel{ nullptr };
        QLabel* rEffLabel{ nullptr };
        QLabel* lEffLabel{ nullptr };
    };
}
