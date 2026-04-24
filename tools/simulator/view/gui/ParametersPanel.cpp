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

        // Calibration results
        auto* calibrationGroup = QtOwned<QGroupBox>("Calibration Results", this);
        auto* calibrationLayout = QtOwned<QGridLayout>();

        calibrationLayout->addWidget(QtOwned<QLabel>("Pole pairs (p):", this), 0, 0);
        polePairsLabel = QtOwned<QLabel>("---", this);
        polePairsLabel->setAlignment(Qt::AlignRight);
        calibrationLayout->addWidget(polePairsLabel, 0, 1);

        calibrationLayout->addWidget(QtOwned<QLabel>("Alignment offset:", this), 1, 0);
        alignmentOffsetLabel = QtOwned<QLabel>("---", this);
        alignmentOffsetLabel->setAlignment(Qt::AlignRight);
        calibrationLayout->addWidget(alignmentOffsetLabel, 1, 1);

        calibrationGroup->setLayout(calibrationLayout);
        layout->addWidget(calibrationGroup);

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

        // ADC Noise
        auto* noiseGroup = QtOwned<QGroupBox>("ADC Noise", this);
        auto* noiseLayout = QtOwned<QGridLayout>();

        noiseLayout->addWidget(QtOwned<QLabel>("Sigma:", this), 0, 0);
        sigmaSpin = QtOwned<QDoubleSpinBox>(this);
        sigmaSpin->setRange(0.0, 500.0);
        sigmaSpin->setSuffix(" mA");
        sigmaSpin->setSingleStep(1.0);
        sigmaSpin->setValue(0.0);
        noiseLayout->addWidget(sigmaSpin, 0, 1);

        noiseLayout->addWidget(QtOwned<QLabel>("Bias A:", this), 1, 0);
        biasASpin = QtOwned<QDoubleSpinBox>(this);
        biasASpin->setRange(-500.0, 500.0);
        biasASpin->setSuffix(" mA");
        biasASpin->setSingleStep(1.0);
        biasASpin->setValue(0.0);
        noiseLayout->addWidget(biasASpin, 1, 1);

        noiseLayout->addWidget(QtOwned<QLabel>("Bias B:", this), 2, 0);
        biasBSpin = QtOwned<QDoubleSpinBox>(this);
        biasBSpin->setRange(-500.0, 500.0);
        biasBSpin->setSuffix(" mA");
        biasBSpin->setSingleStep(1.0);
        biasBSpin->setValue(0.0);
        noiseLayout->addWidget(biasBSpin, 2, 1);

        noiseLayout->addWidget(QtOwned<QLabel>("Bias C:", this), 3, 0);
        biasCSpin = QtOwned<QDoubleSpinBox>(this);
        biasCSpin->setRange(-500.0, 500.0);
        biasCSpin->setSuffix(" mA");
        biasCSpin->setSingleStep(1.0);
        biasCSpin->setValue(0.0);
        noiseLayout->addWidget(biasCSpin, 3, 1);

        noiseGroup->setLayout(noiseLayout);
        layout->addWidget(noiseGroup);

        connect(sigmaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double)
            {
                EmitNoiseConfig();
            });
        connect(biasASpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double)
            {
                EmitNoiseConfig();
            });
        connect(biasBSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double)
            {
                EmitNoiseConfig();
            });
        connect(biasCSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double)
            {
                EmitNoiseConfig();
            });

        // Thermal Config
        auto* thermalGroup = QtOwned<QGroupBox>("Thermal Config", this);
        auto* thermalLayout = QtOwned<QGridLayout>();

        thermalLayout->addWidget(QtOwned<QLabel>(QString::fromUtf8("T_ambient (\xC2\xB0"
                                                                   "C):"),
                                     this),
            0, 0);
        tAmbientSpin = QtOwned<QDoubleSpinBox>(this);
        tAmbientSpin->setRange(-40.0, 150.0);
        tAmbientSpin->setSingleStep(1.0);
        tAmbientSpin->setValue(25.0);
        thermalLayout->addWidget(tAmbientSpin, 0, 1);

        thermalLayout->addWidget(QtOwned<QLabel>(QString::fromUtf8("R_th (\xC2\xB0"
                                                                   "C/W):"),
                                     this),
            1, 0);
        rThSpin = QtOwned<QDoubleSpinBox>(this);
        rThSpin->setRange(0.01, 100.0);
        rThSpin->setSingleStep(0.1);
        rThSpin->setValue(2.0);
        thermalLayout->addWidget(rThSpin, 1, 1);

        thermalLayout->addWidget(QtOwned<QLabel>(QString::fromUtf8("C_th (J/\xC2\xB0"
                                                                   "C):"),
                                     this),
            2, 0);
        cThSpin = QtOwned<QDoubleSpinBox>(this);
        cThSpin->setRange(0.1, 1000.0);
        cThSpin->setSingleStep(0.5);
        cThSpin->setValue(25.0);
        thermalLayout->addWidget(cThSpin, 2, 1);

        thermalLayout->addWidget(QtOwned<QLabel>(QString::fromUtf8("\xCE\xB1"
                                                                   "_Cu (1/\xC2\xB0"
                                                                   "C):"),
                                     this),
            3, 0);
        alphaCuSpin = QtOwned<QDoubleSpinBox>(this);
        alphaCuSpin->setRange(0.0, 0.01);
        alphaCuSpin->setSingleStep(0.0001);
        alphaCuSpin->setDecimals(5);
        alphaCuSpin->setValue(0.00393);
        thermalLayout->addWidget(alphaCuSpin, 3, 1);

        thermalLayout->addWidget(QtOwned<QLabel>(QString::fromUtf8("\xCE\xB2"
                                                                   "_Fe (1/\xC2\xB0"
                                                                   "C):"),
                                     this),
            4, 0);
        betaFeSpin = QtOwned<QDoubleSpinBox>(this);
        betaFeSpin->setRange(-0.001, 0.001);
        betaFeSpin->setSingleStep(0.0001);
        betaFeSpin->setDecimals(5);
        betaFeSpin->setValue(0.0);
        thermalLayout->addWidget(betaFeSpin, 4, 1);

        resetTempButton = QtOwned<QPushButton>("Reset Temperature", this);
        thermalLayout->addWidget(resetTempButton, 5, 0, 1, 2);

        thermalGroup->setLayout(thermalLayout);
        layout->addWidget(thermalGroup);

        connect(tAmbientSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double)
            {
                EmitThermalConfig();
            });
        connect(rThSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double)
            {
                EmitThermalConfig();
            });
        connect(cThSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double)
            {
                EmitThermalConfig();
            });
        connect(alphaCuSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double)
            {
                EmitThermalConfig();
            });
        connect(betaFeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double)
            {
                EmitThermalConfig();
            });
        connect(resetTempButton, &QPushButton::clicked, this, [this]()
            {
                emit thermalResetRequested();
            });

        // Live Thermal
        auto* liveThermalGroup = QtOwned<QGroupBox>("Live Thermal", this);
        auto* liveThermalLayout = QtOwned<QGridLayout>();

        liveThermalLayout->addWidget(QtOwned<QLabel>(QString::fromUtf8("T_winding (\xC2\xB0"
                                                                       "C):"),
                                         this),
            0, 0);
        tWindingLabel = QtOwned<QLabel>("---", this);
        tWindingLabel->setAlignment(Qt::AlignRight);
        liveThermalLayout->addWidget(tWindingLabel, 0, 1);

        liveThermalLayout->addWidget(QtOwned<QLabel>(QString::fromUtf8("R(T) (\xCE\xA9):"), this), 1, 0);
        rEffLabel = QtOwned<QLabel>("---", this);
        rEffLabel->setAlignment(Qt::AlignRight);
        liveThermalLayout->addWidget(rEffLabel, 1, 1);

        liveThermalLayout->addWidget(QtOwned<QLabel>("L_d(T) (mH):", this), 2, 0);
        lEffLabel = QtOwned<QLabel>("---", this);
        lEffLabel->setAlignment(Qt::AlignRight);
        liveThermalLayout->addWidget(lEffLabel, 2, 1);

        liveThermalGroup->setLayout(liveThermalLayout);
        layout->addWidget(liveThermalGroup);

        layout->addStretch();

        // Populate values
        resistanceLabel->setText(QString::number(static_cast<double>(motorParameters.R.Value()), 'g', 4) + QString::fromUtf8(" \xCE\xA9"));
        inductanceLabel->setText(QString::number(static_cast<double>(motorParameters.Ld.Value()), 'g', 4) + " H");
        frictionLabel->setText(QString::number(static_cast<double>(motorParameters.B.Value()), 'g', 4) + QString::fromUtf8(" N\xC2\xB7m\xC2\xB7s/rad"));
        inertiaLabel->setText(QString::number(static_cast<double>(motorParameters.J.Value()), 'g', 4) + QString::fromUtf8(" kg\xC2\xB7m\xC2\xB2"));
        polePairsLabel->setText(QString::number(static_cast<int>(motorParameters.p)));

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

    void ParametersPanel::UpdateResistance(foc::Ohm value)
    {
        resistanceLabel->setText(QString::number(static_cast<double>(value.Value()), 'g', 4) + QString::fromUtf8(" \xCE\xA9"));
    }

    void ParametersPanel::UpdateInductance(foc::MilliHenry value)
    {
        inductanceLabel->setText(QString::number(static_cast<double>(value.Value()), 'g', 4) + " mH");
    }

    void ParametersPanel::UpdateFriction(foc::NewtonMeterSecondPerRadian value)
    {
        frictionLabel->setText(QString::number(static_cast<double>(value.Value()), 'g', 4) + QString::fromUtf8(" N\xC2\xB7m\xC2\xB7s/rad"));
    }

    void ParametersPanel::UpdateInertia(foc::NewtonMeterSecondSquared value)
    {
        inertiaLabel->setText(QString::number(static_cast<double>(value.Value()), 'g', 4) + QString::fromUtf8(" kg\xC2\xB7m\xC2\xB2"));
    }

    void ParametersPanel::UpdatePolePairs(std::size_t value)
    {
        polePairsLabel->setText(QString::number(static_cast<qulonglong>(value)));
    }

    void ParametersPanel::UpdateAlignmentOffset(foc::Radians value)
    {
        alignmentOffsetLabel->setText(QString::number(static_cast<double>(value.Value()), 'g', 4) + " rad");
    }

    void ParametersPanel::EmitNoiseConfig()
    {
        ThreePhaseMotorModel::NoiseConfig c;
        c.sigmaAmpere = static_cast<float>(sigmaSpin->value() / 1000.0);
        c.biasAmpereA = static_cast<float>(biasASpin->value() / 1000.0);
        c.biasAmpereB = static_cast<float>(biasBSpin->value() / 1000.0);
        c.biasAmpereC = static_cast<float>(biasCSpin->value() / 1000.0);
        emit noiseConfigChanged(c);
    }

    void ParametersPanel::EmitThermalConfig()
    {
        ThreePhaseMotorModel::ThermalConfig c;
        c.ambientCelsius = static_cast<float>(tAmbientSpin->value());
        c.thermalResistance = static_cast<float>(rThSpin->value());
        c.thermalCapacitance = static_cast<float>(cThSpin->value());
        c.copperTempCoeff = static_cast<float>(alphaCuSpin->value());
        c.ironInductanceCoeff = static_cast<float>(betaFeSpin->value());
        emit thermalConfigChanged(c);
    }

    void ParametersPanel::UpdateLiveThermal(float tempCelsius, foc::Ohm rEff, foc::Henry lEff)
    {
        tWindingLabel->setText(QString::number(static_cast<double>(tempCelsius), 'f', 1) + QString::fromUtf8(" \xC2\xB0"
                                                                                                             "C"));
        rEffLabel->setText(QString::number(static_cast<double>(rEff.Value()), 'g', 4) + QString::fromUtf8(" \xCE\xA9"));
        lEffLabel->setText(QString::number(static_cast<double>(lEff.Value()) * 1000.0, 'g', 4) + " mH");
    }
}
