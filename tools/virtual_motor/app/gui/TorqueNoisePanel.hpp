#pragma once

#include "core/foc/model/drive/SilPlantConfig.hpp"
#include "tools/virtual_motor/app/logic/Rig.hpp"
#include <QWidget>
#include <array>
#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace virtual_motor::app
{
    class TorqueNoisePanel
        : public QWidget
    {
        Q_OBJECT

    public:
        TorqueNoisePanel(RigMotor& motor, const std::optional<sil::SilPlantConfig>& plant, QWidget* parent = nullptr);

        void Refresh();
        void ShowRefusal(const port::Refused& refused);

    private:
        QWidget* BuildTorque();
        QWidget* BuildNoise();
        QWidget* BuildActive();
        QWidget* BuildMotor();
        QDoubleSpinBox* Spin(double minimum, double maximum, int decimals, const QString& suffix, const QString& name);
        void ApplyOnEnter(QDoubleSpinBox& spin, QPushButton& apply);
        void FormChanged();
        void ApplyExternalTorque();
        void RemoveSelected();
        void ShowInForce(const port::Changes& changes);

        RigMotor& motor;
        std::optional<sil::SilPlantConfig> plant;
        QDoubleSpinBox* loadTorque;
        QComboBox* form;
        std::array<QLabel*, 3> parameterLabels{};
        std::array<QDoubleSpinBox*, 3> parameters{};
        QDoubleSpinBox* currentNoise;
        QDoubleSpinBox* encoderNoise;
        QLabel* inForce;
        QListWidget* active;
        QPushButton* remove;
        QLabel* refusal;
        std::optional<port::Changes> shown;
    };
}
