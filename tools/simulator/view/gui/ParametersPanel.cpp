#include "tools/simulator/view/gui/ParametersPanel.hpp"
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <memory>

namespace simulator
{
    namespace
    {
        template<typename T, typename... Args>
        T* QtOwned(Args&&... args)
        {
            return std::make_unique<T>(std::forward<Args>(args)...).release();
        }
    }

    ParametersPanel::ParametersPanel(const ThreePhaseMotorModel::Parameters& motorParameters, const PidParameters& pidParameters, QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = QtOwned<QVBoxLayout>(this);

        // Electrical parameters
        auto* electricalGroup = QtOwned<QGroupBox>("Electrical Parameters", this);
        auto* electricalLayout = QtOwned<QGridLayout>();

        electricalLayout->addWidget(QtOwned<QLabel>("Resistance (R):", this), 0, 0);
        resistanceLabel = QtOwned<QLabel>("---", this);
        resistanceLabel->setAlignment(Qt::AlignRight);
        electricalLayout->addWidget(resistanceLabel, 0, 1);

        electricalLayout->addWidget(QtOwned<QLabel>("Inductance (L):", this), 1, 0);
        inductanceLabel = QtOwned<QLabel>("---", this);
        inductanceLabel->setAlignment(Qt::AlignRight);
        electricalLayout->addWidget(inductanceLabel, 1, 1);

        electricalGroup->setLayout(electricalLayout);
        layout->addWidget(electricalGroup);

        // Mechanical parameters
        auto* mechanicalGroup = QtOwned<QGroupBox>("Mechanical Parameters", this);
        auto* mechanicalLayout = QtOwned<QGridLayout>();

        mechanicalLayout->addWidget(QtOwned<QLabel>("Friction (B):", this), 0, 0);
        frictionLabel = QtOwned<QLabel>("---", this);
        frictionLabel->setAlignment(Qt::AlignRight);
        mechanicalLayout->addWidget(frictionLabel, 0, 1);

        mechanicalLayout->addWidget(QtOwned<QLabel>("Inertia (J):", this), 1, 0);
        inertiaLabel = QtOwned<QLabel>("---", this);
        inertiaLabel->setAlignment(Qt::AlignRight);
        mechanicalLayout->addWidget(inertiaLabel, 1, 1);

        mechanicalGroup->setLayout(mechanicalLayout);
        layout->addWidget(mechanicalGroup);

        // Current controller gains
        auto* currentPidGroup = QtOwned<QGroupBox>("Current Controller (PI)", this);
        auto* currentPidLayout = QtOwned<QGridLayout>();

        currentPidLayout->addWidget(QtOwned<QLabel>("Kp:", this), 0, 0);
        currentKpLabel = QtOwned<QLabel>("---", this);
        currentKpLabel->setAlignment(Qt::AlignRight);
        currentPidLayout->addWidget(currentKpLabel, 0, 1);

        currentPidLayout->addWidget(QtOwned<QLabel>("Ki:", this), 1, 0);
        currentKiLabel = QtOwned<QLabel>("---", this);
        currentKiLabel->setAlignment(Qt::AlignRight);
        currentPidLayout->addWidget(currentKiLabel, 1, 1);

        currentPidGroup->setLayout(currentPidLayout);
        layout->addWidget(currentPidGroup);

        // Speed controller gains (optional)
        if (pidParameters.speed)
        {
            auto* speedPidGroup = QtOwned<QGroupBox>("Speed Controller (PID)", this);
            auto* speedPidLayout = QtOwned<QGridLayout>();

            speedPidLayout->addWidget(QtOwned<QLabel>("Kp:", this), 0, 0);
            speedKpLabel = QtOwned<QLabel>("---", this);
            speedKpLabel->setAlignment(Qt::AlignRight);
            speedPidLayout->addWidget(speedKpLabel, 0, 1);

            speedPidLayout->addWidget(QtOwned<QLabel>("Ki:", this), 1, 0);
            speedKiLabel = QtOwned<QLabel>("---", this);
            speedKiLabel->setAlignment(Qt::AlignRight);
            speedPidLayout->addWidget(speedKiLabel, 1, 1);

            speedPidLayout->addWidget(QtOwned<QLabel>("Kd:", this), 2, 0);
            speedKdLabel = QtOwned<QLabel>("---", this);
            speedKdLabel->setAlignment(Qt::AlignRight);
            speedPidLayout->addWidget(speedKdLabel, 2, 1);

            speedPidGroup->setLayout(speedPidLayout);
            layout->addWidget(speedPidGroup);
        }

        // Position controller gains (optional)
        if (pidParameters.position)
        {
            auto* positionPidGroup = QtOwned<QGroupBox>("Position Controller (PID)", this);
            auto* positionPidLayout = QtOwned<QGridLayout>();

            positionPidLayout->addWidget(QtOwned<QLabel>("Kp:", this), 0, 0);
            positionKpLabel = QtOwned<QLabel>("---", this);
            positionKpLabel->setAlignment(Qt::AlignRight);
            positionPidLayout->addWidget(positionKpLabel, 0, 1);

            positionPidLayout->addWidget(QtOwned<QLabel>("Ki:", this), 1, 0);
            positionKiLabel = QtOwned<QLabel>("---", this);
            positionKiLabel->setAlignment(Qt::AlignRight);
            positionPidLayout->addWidget(positionKiLabel, 1, 1);

            positionPidLayout->addWidget(QtOwned<QLabel>("Kd:", this), 2, 0);
            positionKdLabel = QtOwned<QLabel>("---", this);
            positionKdLabel->setAlignment(Qt::AlignRight);
            positionPidLayout->addWidget(positionKdLabel, 2, 1);

            positionPidGroup->setLayout(positionPidLayout);
            layout->addWidget(positionPidGroup);
        }

        layout->addStretch();

        // Populate values
        resistanceLabel->setText(QString::number(static_cast<double>(motorParameters.R.Value()), 'g', 4) + QString::fromUtf8(" \xCE\xA9"));
        inductanceLabel->setText(QString::number(static_cast<double>(motorParameters.Ld.Value()), 'g', 4) + " H");
        frictionLabel->setText(QString::number(static_cast<double>(motorParameters.B.Value()), 'g', 4) + QString::fromUtf8(" N\xC2\xB7m\xC2\xB7s/rad"));
        inertiaLabel->setText(QString::number(static_cast<double>(motorParameters.J.Value()), 'g', 4) + QString::fromUtf8(" kg\xC2\xB7m\xC2\xB2"));

        UpdatePidParameters(pidParameters);
    }

    void ParametersPanel::UpdatePidParameters(const PidParameters& pidParameters)
    {
        currentKpLabel->setText(QString::number(static_cast<double>(pidParameters.current.kp), 'g', 6));
        currentKiLabel->setText(QString::number(static_cast<double>(pidParameters.current.ki), 'g', 6));

        if (speedKpLabel && pidParameters.speed)
        {
            speedKpLabel->setText(QString::number(static_cast<double>(pidParameters.speed->kp), 'g', 6));
            speedKiLabel->setText(QString::number(static_cast<double>(pidParameters.speed->ki), 'g', 6));
            speedKdLabel->setText(QString::number(static_cast<double>(pidParameters.speed->kd), 'g', 6));
        }

        if (positionKpLabel && pidParameters.position)
        {
            positionKpLabel->setText(QString::number(static_cast<double>(pidParameters.position->kp), 'g', 6));
            positionKiLabel->setText(QString::number(static_cast<double>(pidParameters.position->ki), 'g', 6));
            positionKdLabel->setText(QString::number(static_cast<double>(pidParameters.position->kd), 'g', 6));
        }
    }
}
