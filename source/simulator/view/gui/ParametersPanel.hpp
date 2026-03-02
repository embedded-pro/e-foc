#pragma once

#include "source/simulator/model/Model.hpp"
#include <QLabel>
#include <QWidget>

namespace simulator
{
    class ParametersPanel
        : public QWidget
    {
        Q_OBJECT

    public:
        struct PidParameters
        {
            float currentKp;
            float currentKi;
            float currentKd;
            float speedKp;
            float speedKi;
            float speedKd;
        };

        ParametersPanel(const ThreePhaseMotorModel::Parameters& motorParameters, const PidParameters& pidParameters, QWidget* parent = nullptr);

        void UpdatePidParameters(const PidParameters& pidParameters);

    private:
        QLabel* resistanceLabel;
        QLabel* inductanceLabel;
        QLabel* frictionLabel;
        QLabel* inertiaLabel;

        QLabel* currentKpLabel;
        QLabel* currentKiLabel;
        QLabel* speedKpLabel;
        QLabel* speedKiLabel;
        QLabel* speedKdLabel;
    };
}
