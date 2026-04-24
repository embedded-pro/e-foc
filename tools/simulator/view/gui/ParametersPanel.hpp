#pragma once

#include "core/foc/interfaces/Units.hpp"
#include "tools/simulator/model/Model.hpp"
#include <QLabel>
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
    };
}
